#include "GirsClient.h"
#include "IrServer.h"
#include "WebServerMgr.h"
#include "WebSocketMgr.h"
#include "WiFiMgr.h"
#include "config.h"

ConfigMgr config;
WiFiMgr wifiMgr;
WebSocketMgr webSocketMgr;
IrServer irServer;
UDPServer udp;
WebUI webUI;
GirsClient girsClient;

#if defined(ESP8266)
#include <SoftwareSerial.h>
SoftwareSerial btSerial(BT_RX_PIN, BT_TX_PIN);
#elif defined(CONFIG_IDF_TARGET_ESP32)
#include <BluetoothSerial.h>
BluetoothSerial btSerial;
// #include "BLESerial/BLESerial.h"
// BLESerial btSerial;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#include "BLESerial/BLESerial.h"
BLESerial btSerial;
#endif


void powerWatchDogTic();
void btnTic();

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // Светодиод включён при старте (активный низкий уровень)
  pinMode(READY_TO_RECEIVE_BTN_PIN, INPUT_PULLUP);
  pinMode(POWER_WATCH_DOG_PIN, OUTPUT);

  Serial.begin(115200);
  while (!Serial);  // Ждём готовности Serial (полезно для отладки)
  delay(500);
  Serial.println("\n\n");

  // Инициализация компонентов
  config.begin();
  wifiMgr.begin();
  webSocketMgr.begin();
  irServer.begin();
  udp.begin(UDP_PORT);
  webUI.begin();
  girsClient.begin();
  girsClient.addStream(&Serial);

#if defined(ESP8266)
  btSerial.begin(9600);
  girsClient.addStream(&btSerial);
#elif defined(ESP32)
  btSerial.begin(config.getUniqueHostname());
  girsClient.addStream(&btSerial);
#endif

  Serial.println("FIRMWARE_VER: " FIRMWARE_VER);
  Serial.println("setup: complete");
  digitalWrite(LED_PIN, HIGH);  // Выключаем светодиод после инициализации
}

void loop() {
  webUI.update();
  /**
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/
  https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/#comment-6525688
  **/
  delay(1);

  wifiMgr.update();
  yield();

  webSocketMgr.update();
  yield();

  udp.update();
  yield();

  irServer.update();
  yield();

  btnTic();
  yield();

  powerWatchDogTic();
  yield();

  girsClient.update();
  yield();
}

void btnTic() {
  // TODO FIXME при использовании IrScrutinizer он постоянно шлет Сигнал DTR в
  // Serial, это приводит к тому что pin D3 на NodeMCU переходит в состояние LOW
  if (digitalRead(READY_TO_RECEIVE_BTN_PIN) == LOW) {
#ifdef ESP32
    readyToReceive();
#endif
#ifdef ESP8266
    // readyToReceive();
#endif
  }
}

void readyToReceive() {
  irServer.resetLastIRData();
  webSocketMgr.notifyReceivedDataSetChanged(irServer.getLastIRData());
  irServer.enableReceiver();
  digitalWrite(LED_PIN, LOW);  // Включаем светодиод (инвертировано)
}

void powerWatchDogTic() {
  static enum { IDLE, PULSE_LOW } state = IDLE;
  static uint32_t lastTime = 0;
  const uint32_t IDLE_INTERVAL = 5000;  // Интервал между импульсами (5 сек)
  const uint32_t PULSE_DURATION = 50;   // Длительность низкого уровня (50 мс)

  uint32_t currentTime = millis();

  switch (state) {
  case IDLE:
    if (currentTime - lastTime >= IDLE_INTERVAL) {
      digitalWrite(POWER_WATCH_DOG_PIN, LOW);
      lastTime = currentTime;
      state = PULSE_LOW;
    }
    break;

  case PULSE_LOW:
    if (currentTime - lastTime >= PULSE_DURATION) {
      digitalWrite(POWER_WATCH_DOG_PIN, HIGH);
      state = IDLE;
    }
    break;
  }
}