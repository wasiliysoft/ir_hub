#include <EEPROM.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>  //3.1.2
#include <ESP8266mDNS.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h>  //2.8.6
#include <IRsend.h>
#include <IRutils.h>
#include <WebSocketsServer.h>  //2.6.1
#include <WiFiUdp.h>

#define FIRMWARE_VER "v2.2.0 (2025.03.01)"

#define SSID_DEFAULT "AutoConnectAP"
#define HOSTNAME "irhub"
#define irLedPin D1  // Пин для ИК светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define recvPin D2   // Пин, к которому подключен ИК-приемник
#define ledPin D4    // Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)


#define UDP_PORT 55531  // Порт для широковещательного UDP

#define DEBUG 0  // 1 для включения отладки, 0 для отключения


ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket = WebSocketsServer(81);

WiFiUDP udp;
char udpBuffer[255];


// Структура для хранения настроек
struct Settings {
  char ssid[32];
  char password[64];
  bool isAPMode;
};
Settings settings;

IRrecv irrecv(recvPin);
IRsend irsend(irLedPin);
decode_results results;  // Буфер для хранения полученных ИК данных
// Переменные для хранения последней полученной ИК-команды
String lastIRCode = "Ожидание сигнала...";
String lastIRProtocol = lastIRCode;
String lastIRRaw = lastIRCode;
bool isWaitingForIR = false;  // Флаг для управления состоянием ИК-приемника

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);  // Включаем светодиод при старте
  delay(1000);
  Serial.println();
  Serial.println();
  delay(1000);

  // EEPROM.begin(sizeof(Settings));
  EEPROM.begin(1023);

  loadSettings();

  initialWiFi();

  // Инициализация mDNS
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Ошибка настройки mDNS!");
  } else {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS запущен, имя хоста: " + String(HOSTNAME) + ".local");
  }

  // Настройка маршрутов веб-сервера
  server.on("/", handleRoot);
  server.on("/reset", handleReset);
  server.on("/sendIr/", HTTP_POST, handleSendRaw);
  server.on("/eraseWiFiCredentials", handleEraseWifiCredentials);
  server.on("/config", handleConfig);
  server.on("/save", handleSave);
  server.on("/scan", handleScan);
  server.onNotFound(handleNotFound);
  server.begin();  // Запуск веб-сервера
  Serial.println("Веб-сервер запущен");

  // Запуск WebSocket
  webSocket.begin();
  Serial.println("WebSocket запущен");

  // Запуск UDP
  udp.begin(UDP_PORT);
  Serial.println("UDP запущен на порту " + String(UDP_PORT));

  irrecv.enableIRIn();  // Инициализация ИК-приемника
  irsend.begin();       // Инициализация ИК-передатчика

  Serial.println("Загрузка завершена");
  Serial.println("Версия прошивки: " + String(FIRMWARE_VER));
  digitalWrite(ledPin, HIGH);  // Выключаем светодиод
}

void loop() {
  // Обработка запросов веб-сервера
  server.handleClient();
  yield();

  // Обработка DNS-запросов в режиме точки доступа
  if (settings.isAPMode) {
    dnsServer.processNextRequest();
  }

  // Обработка событий WebSocket
  webSocket.loop();
  yield();

  // Обработка входящих UDP-пакетов
  handleUDP();
  yield();

  // Обновление mDNS
  MDNS.update();
  yield();

  // Обработка ИК приемника
  doIrReceive();
  yield();
}

void doIrReceive() {
  // Если включен режим ожидания ИК-сигнала
  if (isWaitingForIR && irrecv.decode(&results)) {
    lastIRCode = uint64ToString(results.value, HEX);
    lastIRProtocol = typeToString(results.decode_type);
    lastIRRaw = resultToRawArray(&results);

#if (DEBUG == 1)
    Serial.println("Получен ИК-код: " + lastIRCode + ", Протокол: " + lastIRProtocol);
    Serial.println("RAW данные: " + lastIRRaw);
#endif
    irrecv.pause();
    isWaitingForIR = false;
    String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" + lastIRProtocol + "\",\"raw\":\"" + lastIRRaw + "\"}";
    webSocket.broadcastTXT(jsonData);
    sendUDPBroadcast(jsonData);
    digitalWrite(ledPin, HIGH);
  }
}


// Обработка главной страницы
void handleRoot() {
  String html = "<html><head>";
  html += getCSS();
  html += "<title>ИК-приемник</title>";
  html += "<script>";
  html += "var socket = new WebSocket('ws://' + window.location.hostname + ':81/');";
  html += "socket.onmessage = function(event) {";
  html += "  var data = JSON.parse(event.data);";
  html += "  document.getElementById('ir-code').innerText = data.code;";
  html += "  document.getElementById('ir-protocol').innerText = data.protocol;";
  html += "  document.getElementById('ir-raw').innerText = data.raw;";
  html += "};";
  html += "</script>";
  html += "</head><body>";
  html += "<p>Протокол: <strong id='ir-protocol'>" + lastIRProtocol + "</strong></p>";
  html += "<p>hexcode: <strong id='ir-code'>" + lastIRCode + "</strong></p>";
  html += "<p>RAW данные: <br><strong id='ir-raw'>" + lastIRRaw + "</strong></p>";
  html += "<form action='/reset' method='POST'>";
  html += "<input type='submit' value='СБРОСИТЬ И ПРИГОТОВИТЬСЯ'>";
  html += "</form>";
  html += "<br><br><br><br><hr>";
  html += "<a href='/config'>Настройки</a>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Обработка кнопки "Сбросить и приготовиться"
void handleReset() {
  // Сбрасываем последний ИК код
  lastIRProtocol = lastIRCode = lastIRRaw = "Ожидание сигнала...";
  isWaitingForIR = true;      // Включаем режим ожидания ИК-сигнала
  irrecv.resume();            // Возобновляем работу приемника
  digitalWrite(ledPin, LOW);  // Включаем светодиод (инвертировано)
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
}

// Функция для отправки UDP Broadcast
void sendUDPBroadcast(String message) {
  // Устанавливаем широковещательный адрес
  IPAddress broadcastIP = WiFi.localIP();
  broadcastIP[3] = 255;  // Последний октет адреса устанавливаем в 255 для
                         // широковещательной рассылки

  // Отправляем сообщение
  udp.beginPacket(broadcastIP, UDP_PORT);
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
    if (receivedMessage == "IRHUB_ECHO") {
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
