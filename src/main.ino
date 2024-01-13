#include "main.h"

// TODO Сделать конфиг или аппатную кнопку отключения индикации (возможно даже
// всей вообще)

// WiFi должен пытаться поключиться постоянно а не только при старте
// точка доступа должна запускаться только по нажатию кнопки конфигурации
// по истечении таймера конфигурации должена быть перезагрузка

// FIXME В режиме портала не горит лампочка

void setup(void) {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  LED(1);
  delay(2000);  // ждём старта есп
  Serial.begin(115200);

  EE_startup();
  irsend.begin();
  Serial.printf("IR LED PIN READY %d\n", IR_PIN);

  // Wait for connection or timeout
  if (!startWiFi()) ESP.reset();

  restartHTTP();
  restartUDP();
  LED(0);
}

void loop() {
  server.handleClient();
  UDPTick();
  portalBtnTick();

  if (SP_started) {
    LED(1);
  } else {
    LED(0);
  }
}
