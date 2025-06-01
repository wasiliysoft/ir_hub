#include "config.h"
#include <EEPROM.h>
#include <LittleFS.h>     // https://randomnerdtutorials.com/arduino-ide-2-install-esp8266-littlefs/#installing-windows
#include <ArduinoJson.h>  //7.4.1
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>  //3.1.2
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>  //
#include <IRrecv.h>
#include <IRremoteESP8266.h>  //2.8.6
#include <IRsend.h>
#include <IRutils.h>
#include <WebSocketsServer.h>  //2.6.1
#include <WiFiUdp.h>
#include "GirsClient.h"

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
DNSServer dnsServer;
WebSocketsServer webSocket = WebSocketsServer(81);

WiFiUDP udp;
char udpBuffer[255];

// TODO использваоть girs_sendraw
uint16_t irSendBuf[255];  // буфер для хранения RAW шаблона команды


// Структура для хранения настроек
struct Settings {
  char ssid[32];
  char password[64];
  bool isAPMode;
};
Settings settings;

IRrecv irrecv(IR_RECV_PIN);
IRsend irsend(IR_LED_PIN);
decode_results results;  // Буфер для хранения полученных ИК данных

// Переменные для хранения последней полученной ИК-команды
String lastIRCode = "Ожидание сигнала...";
String lastIRProtocol = lastIRCode;
String lastIRRaw = lastIRCode;
bool isWaitingForIR = false;  // Флаг для управления состоянием ИК-приемника

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Включаем светодиод при старте
  pinMode(READY_TO_RECEIVE_BTN_PIN, INPUT_PULLUP);
  pinMode(POWER_WATCH_DOG_PIN, OUTPUT);
  delay(500);
  Serial.println();
  Serial.println();

  // EEPROM.begin(sizeof(Settings));
  EEPROM.begin(1023);

  loadSettings();

  initialWiFi();

  // Инициализация mDNS
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Ошибка настройки mDNS!");
  } else {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS запущен, имя хоста: http://" + String(HOSTNAME) + ".local");
  }

  LittleFS.begin();
  // Настройка маршрутов веб-сервера
  httpUpdater.setup(&server);  // OTA url /update
  server.onNotFound([]() {
    if (!LittleFS.exists("/index.html")) {
      Serial.println("Файловая система не найдена!");
      server.sendHeader("Location", "/update");
    } else {
      server.sendHeader("Location", "/");
    }
    server.send(302, "text/plain", "Redirecting");
  });
  server.on("/api/v1/last-received-data", HTTP_GET, handleAPI_last_received_data);
  server.on("/api/v1/scan-network", HTTP_GET, handleAPI_scan_network);
  server.on("/api/v1/config-read", HTTP_GET, handleAPI_config_read);
  server.on("/api/v1/config-write", HTTP_POST, handleAPI_config_write);
  server.on("/api/v1/config-erase", HTTP_GET, handleAPI_config_erase);
  server.on("/reset", handleReset);
  server.on("/sendIr/", HTTP_POST, handleSendRaw);
  server.serveStatic("/", LittleFS, "/", "max-age=86400");  // 1 сутки = 24 * 3600 = 86400
  server.begin();                                           // Запуск веб-сервера
  Serial.println("Веб-сервер запущен");

  // Запуск WebSocket
  webSocket.begin();
  Serial.println("WebSocket запущен");

  // Запуск UDP
  udp.begin(UDP_PORT);
  Serial.println("UDP запущен на порту " + String(UDP_PORT));

  irrecv.enableIRIn();  // Инициализация ИК-приемника
  irsend.begin();       // Инициализация ИК-передатчика

  girs_begin();

  Serial.println("Загрузка завершена");
  Serial.println("Версия прошивки: " + String(FIRMWARE_VER));
  digitalWrite(LED_PIN, HIGH);  // Выключаем светодиод
}

void loop() {
  // Обработка запросов веб-сервера
  server.handleClient();
  /**
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/#comment-6525688
  **/
  delay(1);

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

  // Обработка кнопок
  btnTic();
  yield();

  powerWatchDogTic();
  yield();

  girs_tic();
  yield();
}

void doIrReceive() {
  // Если включен режим ожидания ИК-сигнала
  if (isWaitingForIR && irrecv.decode(&results)) {
    lastIRCode = uint64ToString(results.value, HEX);
    lastIRProtocol = typeToString(results.decode_type);
    lastIRRaw = resultToRawArray(&results);
    DEBUG_PRINTF("Получен ИК-код: %s, Протокол:  %s", lastIRCode, lastIRProtocol);
    DEBUG_PRINTF("RAW данные: %s", lastIRRaw);
    irrecv.pause();
    isWaitingForIR = false;
    notifyReceivedDataSetChanged();
    digitalWrite(LED_PIN, HIGH);
  }
}

void btnTic() {
  if (digitalRead(READY_TO_RECEIVE_BTN_PIN) == LOW) {
    handleReset();
  }
}

void notifyReceivedDataSetChanged() {
  String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" + lastIRProtocol + "\",\"raw\":\"" + lastIRRaw + "\"}";
  webSocket.broadcastTXT(jsonData);
}

// Обработчик главной страницы (отдает статический HTML)
void handleRoot() {
  server.send(200, "text/html", "<html><head><meta http-equiv=\"refresh\" content=\"0; url=/index.html\"></head></html>");
}

void handleAPI_last_received_data() {
  DynamicJsonDocument doc(2048);
  doc["protocol"] = lastIRProtocol;
  doc["code"] = lastIRCode;
  doc["raw"] = lastIRRaw;

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAPI_config_read() {
  DynamicJsonDocument doc(512);
  doc["w_ssid"] = settings.ssid;
  doc["w_pass"] = settings.password;
  doc["w_ap"] = settings.isAPMode;
  doc["fw_ver_name"] = FIRMWARE_VER;
  // Добавляем новые поля
  doc["local_ip"] = WiFi.localIP().toString();
  doc["mac_address"] = WiFi.macAddress();
  doc["hostname"] = WiFi.hostname();
  // doc["subnet_mask"] = WiFi.subnetMask().toString();
  // doc["gateway_ip"] = WiFi.gatewayIP().toString();
  // doc["dns_ip"] = WiFi.dnsIP().toString();
  doc["rssi"] = WiFi.RSSI();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleAPI_config_write() {
  if (server.method() == HTTP_POST) {
    settings.isAPMode = server.arg("mode").toInt() == 1;
    strncpy(settings.ssid, server.arg("ssid").c_str(), sizeof(settings.ssid));
    strncpy(settings.password, server.arg("password").c_str(), sizeof(settings.password));
    saveSettings();

    DynamicJsonDocument doc(128);
    doc["status"] = "ok";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
    delay(1000);
    ESP.restart();
  }
}

void handleAPI_config_erase() {
  DynamicJsonDocument doc(128);
  doc["status"] = "ok";

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);

  Serial.println("Сброс параметров подключения WiFi");
  setDefaultSettings();
  saveSettings();
  Serial.println("Перезагрузка...");
  delay(2000);
  ESP.reset();
}


void handleAPI_scan_network() {
  DynamicJsonDocument doc(256);

  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; ++i) {
    doc[i] = WiFi.SSID(i);
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

// Обработка кнопки "Сбросить и приготовиться"
void handleReset() {
  // Сбрасываем последний ИК код
  lastIRProtocol = lastIRCode = lastIRRaw = "Ожидание сигнала...";
  notifyReceivedDataSetChanged();
  isWaitingForIR = true;       // Включаем режим ожидания ИК-сигнала
  irrecv.resume();             // Возобновляем работу приемника
  digitalWrite(LED_PIN, LOW);  // Включаем светодиод (инвертировано)
  // Перенаправляем на главную страницу
  server.sendHeader("Location", "/");
  server.send(303);
}


void handleSendRaw() {
  digitalWrite(LED_PIN, LOW);
  uint16_t hz = server.arg("freq").toInt();
  String pattern = server.arg("patt");

  int pulses = 0;
  int startIndex = 0;
  for (int i = 0; i <= pattern.length(); i++) {
    if (i == pattern.length() || pattern.charAt(i) == ',') {
      irSendBuf[pulses++] = pattern.substring(startIndex, i).toInt();
      startIndex = i + 1;
    }
    yield();
  }

  // отправляем на все узлы в сети
  sendUDPRawIR(hz, pulses);
  yield();
  // отправляем команду на ИК диод
  irsend.sendRaw(irSendBuf, pulses, hz);
  yield();
  server.send(200, "text/plain", "success");
  digitalWrite(LED_PIN, HIGH);
}

/// Ретрансляция irSendBuf на остальне узлы сети по UDP
/// @param[in] hz несущая частота
/// @param[in] pulses размер выборки из буфера
void sendUDPRawIR(uint16_t hz, uint16_t pulses) {
  char type[10] = "IRHUB_S01";

  // Заголовок 14 байт
  // 10 байт (тип) + 2 байта (hz) + 2 байта (длина) + данные
  uint8_t packet[14 + pulses * 2];

  memcpy(packet, type, 10);           // Тип пакета и версия формата
  packet[10] = (hz >> 8) & 0xFF;      // Старший байт hz
  packet[11] = hz & 0xFF;             // Младший байт hz
  packet[12] = (pulses >> 8) & 0xFF;  // Старший байт длины
  packet[13] = pulses & 0xFF;         // Младший байт длины

  for (uint16_t i = 0; i < pulses; i++) {
    packet[14 + i * 2] = (irSendBuf[i] >> 8) & 0xFF;
    packet[14 + i * 2 + 1] = irSendBuf[i] & 0xFF;
  }

  // Устанавливаем широковещательный адрес
  IPAddress broadcastIP = WiFi.localIP();
  broadcastIP[3] = 255;  // Последний октет адреса устанавливаем в 255 для
                         // широковещательной рассылки

  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.write(packet, sizeof(packet));
  udp.endPacket();
  DEBUG_PRINTF("Отправлен UDP Broadcast RAW IR, размер пакета: %i", sizeof(packet));
}

// Функция для обработки входящих UDP-пакетов
void handleUDP() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  if (udp.remoteIP() == WiFi.localIP()) {  // Игнорируем свои же пакеты
    DEBUG_PRINTF("Получен свой UDP broadcast, игнорируем");
    return;
  }

  // Чтение данных из пакета
  int len = udp.read(udpBuffer, sizeof(udpBuffer) - 1);  // Оставляем место для нуль-терминатора
  if (len <= 0) return;

  udpBuffer[len] = '\0';  // Добавляем завершающий нулевой символ

  if (strcmp(udpBuffer, "IRHUB_ECHO") == 0) {
    String macAddress = WiFi.macAddress();
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(macAddress.c_str(), macAddress.length());
    udp.endPacket();
    DEBUG_PRINTF("Отправлен MAC-адрес: %s на IP: %s", macAddress.c_str(), udp.remoteIP().toString().c_str());
  } else if (memcmp(udpBuffer, "IRHUB_S01", 10) == 0) {
    digitalWrite(LED_PIN, LOW);
    DEBUG_PRINTF("Получена ИК команда от IP: %s", udp.remoteIP().toString().c_str());
    uint16_t hz = 0;
    uint16_t pulses = 0;
    hz = (udpBuffer[10] << 8) | udpBuffer[11];
    pulses = (udpBuffer[12] << 8) | udpBuffer[13];
    for (uint16_t i = 0; i < pulses; i++) {
      irSendBuf[i] = (udpBuffer[14 + i * 2] << 8) | udpBuffer[14 + i * 2 + 1];
    }
    yield();
    // отправляем команду на ИК диод
    irsend.sendRaw(irSendBuf, pulses, hz);
    yield();
    DEBUG_PRINTF("hz: %i", hz);
    DEBUG_PRINTF("pulses: %i", pulses);
    DEBUG_PRINTF("irSendBuf[0]: %i", irSendBuf[0]);
    DEBUG_PRINTF("irSendBuf[pulses-1]: %i", irSendBuf[pulses - 1]);
    digitalWrite(LED_PIN, HIGH);
  }
}
