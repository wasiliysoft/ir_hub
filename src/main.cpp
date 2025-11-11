#include "GirsClient.h"
#include "IrServer.h"
#include "WebServerMgr.h"
#include "WebSocketMgr.h"
#include "WiFiMgr.h"
#include "config.h"

ConfigMgr config;
WiFiMgr wifiMgr;
WebUI webUI;
UDPServer udp;
IrServer irServer;
WebSocketMgr webSocketMgr;
GirsClient girsClient;

#ifdef BT_HC06
#include <SoftwareSerial.h>
SoftwareSerial BTserial(BT_RX_PIN, BT_TX_PIN);
#endif

void powerWatchDogTic();
void btnTic();

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
  webUI.begin();
  webSocketMgr.begin();
  Serial.println("WebSocket запущен");
  udp.begin(UDP_PORT);
  irServer.begin();
  girsClient.begin();
  girsClient.addStream(&Serial);

#ifdef BT_HC06
  BTserial.begin(9600); // Стандартная скорость HC-06
  girsClient.addStream(&BTserial);
#endif

  Serial.println("Загрузка завершена");
  Serial.println("Версия прошивки: " + String(FIRMWARE_VER));
  Serial.println("setup section complete");
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
  yield();

  // Обработка событий WebSocket
  webSocketMgr.update();
  yield();

  // Обработка входящих UDP-пакетов
  udp.update();
  yield();

  // Обработка ИК приемника
  irServer.update();
  yield();

  // Обработка кнопок
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
    // readyToReceive();
  }
}

// Обработка кнопки "Сбросить и приготовиться"
void readyToReceive() {
  // Сбрасываем последний ИК код
  irServer.resetLastIRData();
  webSocketMgr.notifyReceivedDataSetChanged(irServer.getLastIRData());
  irServer.enableReceiver();
  digitalWrite(LED_PIN, LOW); // Включаем светодиод (инвертировано)W
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