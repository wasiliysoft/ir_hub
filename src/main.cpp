#include "GirsClient.h"
#include "config.h"

#include <IRrecv.h>
#include <IRremoteESP8266.h> //2.8.6
#include <IRsend.h>
#include <IRutils.h>

#include "UDPServer.h"
#include "WebServer.h"
#include "WiFiMgr.h"
#include <WebSocketsServer.h>


WebSocketsServer webSocket = WebSocketsServer(81);
Config config;
WiFiMgr wifiMgr;
WebUI webUI;
UDPServer udp;

// TODO использваоть girs_sendraw
uint16_t irSendBuf[255]; // буфер для хранения RAW шаблона команды

IRrecv irrecv(IR_RECV_PIN);
IRsend irsend(IR_LED_PIN);
decode_results results; // Буфер для хранения полученных ИК данных

// Переменные для хранения последней полученной ИК-команды
String lastIRCode = "Ожидание сигнала...";
String lastIRProtocol = lastIRCode;
String lastIRRaw = lastIRCode;
bool isWaitingForIR = false; // Флаг для управления состоянием ИК-приемника

void powerWatchDogTic();
void doIrReceive();
void btnTic();
void notifyReceivedDataSetChanged();
void handleUDP();

String resultToRawArray(decode_results *results);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // Включаем светодиод при старте
  pinMode(READY_TO_RECEIVE_BTN_PIN, INPUT_PULLUP);
  pinMode(POWER_WATCH_DOG_PIN, OUTPUT);
  delay(500);
  Serial.println();
  Serial.println();

  config.begin();
  wifiMgr.begin();

  // Инициализация mDNS
  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Ошибка настройки mDNS!");
  } else {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS запущен, имя хоста: http://" + String(HOSTNAME) +
                   ".local");
  }

  webUI.begin();

  // Запуск WebSocket
  webSocket.begin();
  Serial.println("WebSocket запущен");

  // Запуск UDP
  udp.begin(UDP_PORT);
  Serial.println("UDP запущен на порту " + String(UDP_PORT));

  irrecv.enableIRIn(); // Инициализация ИК-приемника
  irsend.begin();      // Инициализация ИК-передатчика

  girs_begin();

  Serial.println("Загрузка завершена");
  Serial.println("Версия прошивки: " + String(FIRMWARE_VER));
  digitalWrite(LED_PIN, HIGH); // Выключаем светодиод
}

void loop() {
  webUI.update();
  /**
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/#comment-6525688
  **/
  delay(1);

  wifiMgr.uopdate();

  // Обработка событий WebSocket
  webSocket.loop();
  yield();

  // Обработка входящих UDP-пакетов
  udp.update();
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
    DEBUG_PRINTF("Получен ИК-код: %s, Протокол:  %s", lastIRCode,
                 lastIRProtocol);
    DEBUG_PRINTF("RAW данные: %s", lastIRRaw);
    irrecv.pause();
    isWaitingForIR = false;
    notifyReceivedDataSetChanged();
    digitalWrite(LED_PIN, HIGH);
  }
}

void btnTic() {
  if (digitalRead(READY_TO_RECEIVE_BTN_PIN) == LOW) {
    readyToReceive();
  }
}

// Обработка кнопки "Сбросить и приготовиться"
void readyToReceive() {
  // Сбрасываем последний ИК код
  lastIRProtocol = lastIRCode = lastIRRaw = "Ожидание сигнала...";
  notifyReceivedDataSetChanged();
  isWaitingForIR = true;      // Включаем режим ожидания ИК-сигнала
  irrecv.resume();            // Возобновляем работу приемника
  digitalWrite(LED_PIN, LOW); // Включаем светодиод (инвертировано)W
}

void notifyReceivedDataSetChanged() {
  String jsonData = "{\"code\":\"" + lastIRCode + "\",\"protocol\":\"" +
                    lastIRProtocol + "\",\"raw\":\"" + lastIRRaw + "\"}";
  webSocket.broadcastTXT(jsonData);
}

// Функция для преобразования RAW данных в строку
String resultToRawArray(decode_results *results) {
  String rawData = "";
  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t usecs;
    for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX;
         usecs -= UINT16_MAX) {
      rawData += uint64ToString(UINT16_MAX);
      rawData += ",";
    }
    rawData += uint64ToString(usecs);
    if (i < results->rawlen - 1)
      rawData += ","; // Добавляем запятую, кроме последнего элемента
  }
  return rawData;
}

void powerWatchDogTic() {
  static enum { IDLE, PULSE_LOW } state = IDLE;
  static uint32_t lastTime =
      0; // вызывается только при инициализации переменной

  uint32_t currentTime = millis();

  switch (state) {
  case IDLE:
    if (currentTime - lastTime >= 5000) {
      digitalWrite(POWER_WATCH_DOG_PIN, LOW);
      lastTime = currentTime;
      state = PULSE_LOW;
    }
    break;

  case PULSE_LOW:
    if (currentTime - lastTime >= 50) {
      digitalWrite(POWER_WATCH_DOG_PIN, HIGH);
      state = IDLE;
    }
    break;
  }
}