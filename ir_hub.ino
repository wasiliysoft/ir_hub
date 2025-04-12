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

#define FIRMWARE_VER "v2.4.0 (2025.04.12)"

#define SSID_DEFAULT "AutoConnectAP"
#define HOSTNAME "irhub"
#define irLedPin D1           // Пин для ИК светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define recvPin D2            // Пин, к которому подключен ИК-приемник
#define handleResetBtnPin D3  // Пин для кнопки "Сброс и приготовиться"
#define ledPin D4             // Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)


#define UDP_PORT 55531  // Порт для широковещательного UDP

#define DEBUG 0  // 1 для включения отладки, 0 для отключения

#if (DEBUG == 1)
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

ESP8266WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket = WebSocketsServer(81);

WiFiUDP udp;
char udpBuffer[255];

uint16_t irSendBuf[255];  // буфер для хранения RAW шаблона команды


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
  pinMode(handleResetBtnPin, INPUT_PULLUP);
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

  // Обработка кнопок
  btnTic();
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
    digitalWrite(ledPin, HIGH);
  }
}

void btnTic() {
  if (digitalRead(handleResetBtnPin) == LOW) {
    handleReset();
  }
}

void notifyReceivedDataSetChanged() {
  String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" + lastIRProtocol + "\",\"raw\":\"" + lastIRRaw + "\"}";
  webSocket.broadcastTXT(jsonData);
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
  notifyReceivedDataSetChanged();
  isWaitingForIR = true;      // Включаем режим ожидания ИК-сигнала
  irrecv.resume();            // Возобновляем работу приемника
  digitalWrite(ledPin, LOW);  // Включаем светодиод (инвертировано)
  // Перенаправляем на главную страницу
  server.sendHeader("Location", "/");
  server.send(303);
}


void handleSendRaw() {
  digitalWrite(ledPin, LOW);
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
  digitalWrite(ledPin, HIGH);
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
    digitalWrite(ledPin, LOW);
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
    digitalWrite(ledPin, HIGH);
  }
}
