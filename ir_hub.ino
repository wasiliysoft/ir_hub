#include <ESP8266WiFi.h>        //3.1.2
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>        //2.0.17
#include <WebSocketsServer.h>   //2.6.1
#include <IRremoteESP8266.h>    //2.8.6
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>        // Добавляем поддержку mDNS

// Определяем флаг DEBUG
#define DEBUG 0  // 1 для включения отладки, 0 для отключения

// Пин, к которому подключен ИК-приемник
#define recvPin D2

// Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)
#define ledPin D4

// Пин для светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define irLedPin D1

// Создаем объект для работы с ИК-приемником
IRrecv irrecv(recvPin);

// Создаем объект для работы с ИК-передатчиком
IRsend irsend(irLedPin);

// Буфер для хранения полученных данных
decode_results results;

// Создаем объект веб-сервера
ESP8266WebServer server(80);

// Создаем объект WebSocket
WebSocketsServer webSocket = WebSocketsServer(81);

// Создаем объект для работы с UDP
WiFiUDP udp;

// Переменные для хранения последней полученной ИК-команды и протокола
String lastIRCode = "Ожидание сигнала...";
String lastIRProtocol = "Неизвестно";

// Флаг для управления состоянием ИК-приемника
bool isWaitingForIR = false;

// Порт для широковещательного UDP
const int udpPort = 55531;

// Буфер для хранения входящих UDP-пакетов
char udpBuffer[255];

void setup() {
  // Инициализация последовательного порта для отладки
  Serial.begin(115200);

  // Настройка пина светодиода как выхода
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);  // Включаем светодиод при старте (инвертировано)

  // Инициализация ИК-приемника
  irrecv.enableIRIn();

  // Инициализация WiFiManager
  WiFiManager wifiManager;

  // Запуск портала настройки Wi-Fi
  // Если устройство не может подключиться к сохраненной сети, оно запустит точку доступа (AP)
  // с именем "AutoConnectAP" и перенаправит на страницу настройки.
  if (!wifiManager.autoConnect("AutoConnectAP")) {
    #if (DEBUG == 1)
    Serial.println("Не удалось подключиться к Wi-Fi и запустить портал настройки.");
    #endif
    delay(3000);
    // Перезагрузка устройства
    ESP.restart();
  }

  #if (DEBUG == 1)
  Serial.println("Подключено к Wi-Fi");
  Serial.println(WiFi.localIP());
  #endif

  // Инициализация mDNS с именем хоста "irhub.local"
  if (!MDNS.begin("irhub")) {
    #if (DEBUG == 1)
    Serial.println("Ошибка настройки mDNS!");
    #endif
  } else {
    #if (DEBUG == 1)
    Serial.println("mDNS запущен, имя хоста: irhub.local");
    #endif
  }

  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);                       // Главная страница
  server.on("/reset", handleReset);                 // Обработка кнопки "Сброс"
  server.on("/sendIr/", HTTP_POST, handleSendRaw);  // отправка RAW

  // Запуск веб-сервера
  server.begin();
  #if (DEBUG == 1)
  Serial.println("Веб-сервер запущен");
  #endif

  // Запуск WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  #if (DEBUG == 1)
  Serial.println("WebSocket запущен");
  #endif

  // Запуск UDP
  udp.begin(udpPort);
  #if (DEBUG == 1)
  Serial.println("UDP запущен на порту " + String(udpPort));
  #endif
}

void loop() {
  // Обработка запросов веб-сервера
  server.handleClient();

  // Обработка событий WebSocket
  webSocket.loop();

  // Обработка входящих UDP-пакетов
  handleUDP();

  // Обновление mDNS
  MDNS.update();

  // Если включен режим ожидания ИК-сигнала
  if (isWaitingForIR && irrecv.decode(&results)) {
    // Сохраняем полученный код и протокол
    lastIRCode = uint64ToString(results.value, HEX);
    lastIRProtocol = typeToString(results.decode_type);
    #if (DEBUG == 1)
    Serial.println("Получен ИК-код: " + lastIRCode + ", Протокол: " + lastIRProtocol);
    #endif

    // Отключаем режим ожидания
    isWaitingForIR = false;

    // Включаем светодиод (инвертировано)
    digitalWrite(ledPin, HIGH);

    // Приостанавливаем приемник
    irrecv.pause();

    // Отправляем данные через WebSocket
    String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" + lastIRProtocol + "\"}";
    webSocket.broadcastTXT(jsonData);

    // Отправляем данные через UDP Broadcast
    sendUDPBroadcast(jsonData);
  }
}

// Обработка главной страницы
void handleRoot() {
  String html = "<html><head>";
  html += "<meta charset='UTF-8'>";  // Добавляем кодировку UTF-8
  html += "<title>ИК-приемник</title>";
  html += "<script>";
  html += "var socket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "socket.onmessage = function(event) {";
  html += "  var data = JSON.parse(event.data);";
  html += "  document.getElementById('ir-code').innerText = data.code;";
  html += "  document.getElementById('ir-protocol').innerText = data.protocol;";
  html += "};";
  html += "</script>";
  html += "</head><body>";
  html += "<p>Последний полученный ИК-код: <strong id='ir-code'>" + lastIRCode + "</strong></p>";
  html += "<p>Протокол: <strong id='ir-protocol'>" + lastIRProtocol + "</strong></p>";
  html += "<form action='/reset' method='POST'>";
  html += "<button type='submit'>Сбросить и приготовиться</button>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Обработка кнопки "Сброс"
void handleReset() {
  // Сбрасываем последний ИК-код и протокол
  lastIRCode = "Ожидание сигнала...";
  lastIRProtocol = "Неизвестно";

  // Включаем режим ожидания ИК-сигнала
  isWaitingForIR = true;

  // Выключаем светодиод (инвертировано)
  digitalWrite(ledPin, LOW);

  // Возобновляем работу приемника
  irrecv.resume();

  // Перенаправляем на главную страницу
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSendRaw() {
  digitalWrite(ledPin, LOW);
  float freq = server.arg("freq").toInt() / 1000;
  char separator = ',';
  String pattern = server.arg("patt");

  // Определяем количество импульсов и парсим строку за один проход
  uint16_t pulses = 1;  // Минимум один импульс
  for (int i = 0; i < pattern.length(); i++) {
    if (pattern.charAt(i) == separator) {
      pulses++;
    }
    yield();
  }

  uint16_t buf[pulses];
  int pulseIndex = 0;
  int startIndex = 0;
  for (int i = 0; i <= pattern.length(); i++) {
    if (i == pattern.length() || pattern.charAt(i) == separator) {
      buf[pulseIndex++] = pattern.substring(startIndex, i).toInt();
      startIndex = i + 1;
    }
    yield();
  }

  irsend.sendRaw(buf, pulses, freq);
  yield();
  server.send(200, "text/plain", "success");
  digitalWrite(ledPin, HIGH);
  yield();
}

// Обработка событий WebSocket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      #if (DEBUG == 1)
      Serial.printf("[%u] Отключен\n", num);
      #endif
      break;
    case WStype_CONNECTED:
      #if (DEBUG == 1)
      Serial.printf("[%u] Подключен\n", num);
      #endif
      break;
  }
}

// Функция для отправки UDP Broadcast
void sendUDPBroadcast(String message) {
  // Устанавливаем широковещательный адрес
  IPAddress broadcastIP = WiFi.localIP();
  broadcastIP[3] = 255;  // Последний октет адреса устанавливаем в 255 для широковещательной рассылки

  // Отправляем сообщение
  udp.beginPacket(broadcastIP, udpPort);
  udp.write(message.c_str(), message.length());
  udp.endPacket();

  #if (DEBUG == 1)
  Serial.println("Отправлено UDP Broadcast: " + message);
  #endif
}

// Функция для обработки входящих UDP-пакетов
void handleUDP() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    // Чтение данных из пакета
    int len = udp.read(udpBuffer, sizeof(udpBuffer));
    if (len > 0) {
      udpBuffer[len] = 0;  // Добавляем завершающий нулевой символ
    }

    // Преобразуем буфер в строку
    String receivedMessage = String(udpBuffer);

    // Проверяем, содержит ли пакет текст "IRHUB_ECHO"
    if (receivedMessage == "IRHUB_ECHO") {
      // Получаем MAC-адрес платы
      String macAddress = WiFi.macAddress();

      // Отправляем MAC-адрес обратно отправителю
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write(macAddress.c_str(), macAddress.length());
      udp.endPacket();

      #if (DEBUG == 1)
      Serial.println("Отправлен MAC-адрес: " + macAddress + " на IP: " + udp.remoteIP().toString());
      #endif
    }
  }
}