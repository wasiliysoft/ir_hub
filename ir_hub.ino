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

#define INIT_ADDR 1023  // номер ячейки для хранения клоюча первого запуска
#define INIT_KEY 53     // ключ первого запуска. 0-254, на выбор

#define SSID_DEFAULT "AutoConnectAP"

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


ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket = WebSocketsServer(81);
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

// Структура для хранения настроек
struct Settings {
  char ssid[32];
  char password[64];
  bool isAPMode;
};

Settings settings;

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

  // Запуск WiFi
  if (settings.isAPMode) {
    startAPMode();
  } else {
    startClientMode();
  }


  // Инициализация mDNS
  if (!MDNS.begin("irhub")) {
    Serial.println("Ошибка настройки mDNS!");
  } else {
    Serial.println("mDNS запущен, имя хоста: irhub.local");
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
  udp.begin(udpPort);
  Serial.println("UDP запущен на порту " + String(udpPort));

  irrecv.enableIRIn();  // Инициализация ИК-приемника
  irsend.begin();       // Инициализация ИК-передатчика

  digitalWrite(ledPin, HIGH);  // Выключаем светодиод
  Serial.println("Загрузка завершена");
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
    // Сохраняем полученный код и протокол
    lastIRCode = uint64ToString(results.value, HEX);
    lastIRProtocol = typeToString(results.decode_type);
#if (DEBUG == 1)
    Serial.println("Получен ИК-код: " + lastIRCode + ", Протокол: " + lastIRProtocol);
#endif

    // Приостанавливаем приемник
    irrecv.pause();

    // Отключаем режим ожидания
    isWaitingForIR = false;

    // Выключаем светодиод (инвертировано)
    digitalWrite(ledPin, HIGH);

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
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ИК-приемник</title>";
  html += "<script>";
  html += "var socket = new WebSocket('ws://' + window.location.hostname + "
          "':81/');";
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
  html += "<br><br><br><br><hr>";
  html += "<a href='/config'>Настройки</a>";
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

  // Включаем светодиод (инвертировано)
  digitalWrite(ledPin, LOW);

  // Возобновляем работу приемника
  irrecv.resume();

  // Перенаправляем на главную страницу
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleConfig() {
  String ssidParam = server.arg("ssid");  // Получаем SSID из параметра URL
  String html = "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ИК-приемник</title>";
  html += "<script>";
  html += "function setSSID(ssid) {";
  html += "  document.getElementsByName('ssid')[0].value = ssid;";
  html += "  document.getElementsByName('mode')[0].value = '0';";
  html += "}";
  html += "</script>";
  html += "</head>";
  html += "<body><h1>WiFi Settings</h1>";
  html += "<form action='/save' method='POST'>";
  html += "Mode: <select name='mode'>";
  html += "<option value='0'" + String(settings.isAPMode ? "" : " selected") + ">Client</option>";
  html += "<option value='1'" + String(settings.isAPMode ? " selected" : "") + ">Access Point</option>";
  html += "</select><br>";
  html += "SSID: <input type='text' name='ssid' value='" + String(settings.ssid) + "'><br>";
  html += "Password: <input type='password' name='password' value='" + String(settings.password) + "'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<hr>";
  html += "<p><a href='/scan'>Сканировать доступные сети</a></p>";  // Ссылка для сканирования сетей
  html += "<br><br><br><br>";
  html += "<p><a href='/eraseWiFiCredentials'>Сбросить настройки WiFi</a></p>";
  html += "</body></html>";

  if (ssidParam.length() > 0) {
    html += "<script>setSSID('" + ssidParam + "');</script>";  // Вызываем функцию для подстановки SSID
  }

  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.method() == HTTP_POST) {
    settings.isAPMode = server.arg("mode").toInt() == 1;
    strncpy(settings.ssid, server.arg("ssid").c_str(), sizeof(settings.ssid));
    strncpy(settings.password, server.arg("password").c_str(), sizeof(settings.password));
    saveSettings();
    server.send(200, "text/plain", "Settings saved. Restarting...");
    delay(1000);
    ESP.restart();
  }
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String html = "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Scan Networks</title>";
  html += "</head>";
  html += "<body><h1>Available Networks</h1>";
  html += "<ul>";
  for (int i = 0; i < n; ++i) {
    html += "<li><a href='/config?ssid=" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</a></li>";
  }
  html += "</ul>";
  html += "<p><a href='/config'>назад</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleNotFound() {
  // Перенаправляем все неизвестные запросы на главную страницу
  IPAddress ip = WiFi.softAPIP();
  String ipString = String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
  String url = "http://" + ipString + "/";
  server.sendHeader("Location", url, true);
  server.send(302, "text/plain", "");
}

void handleEraseWifiCredentials() {
  // Перенаправляем на главную страницу
  server.sendHeader("Location", "/");
  server.send(303);

  Serial.println("Сброс параметров подключения WiFi");
  setDefaultSettings();
  saveSettings();
  Serial.println("Перезагрузка...");
  delay(2000);
  ESP.reset();
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

void loadSettings() {
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) {  // первый запуск
    delay(5000);
    Serial.println("Первый запуск");
    Serial.println("Первый запуск");
    Serial.println("Первый запуск");
    EEPROM.write(INIT_ADDR, INIT_KEY);  // записали ключ
    setDefaultSettings();
    saveSettings();
  } else {
    EEPROM.get(0, settings);
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void setDefaultSettings() {
  strcpy(settings.ssid, SSID_DEFAULT);
  strcpy(settings.password, "");
  settings.isAPMode = true;
}

void startClientMode() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledPin, LOW);
    delay(400);
    digitalWrite(ledPin, HIGH);
    delay(400);
    Serial.print(".");
    if (millis() > 10000) {
      Serial.println("Не удается подключиться к сети, включаем точку доступа");
      setDefaultSettings();
      startAPMode();
      return;
    }
  }
  Serial.println("");
  Serial.println("WiFi подключен");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(settings.ssid, settings.password);
  Serial.println("Точка доступа запущена");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  // Запуск DNS-сервера для перенаправления всех запросов на IP ESP8266
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("DNS сервер запущен");
}