#include "main.h"

// TODO Сделать конфиг или аппатную кнопку отключения индикации
// (возможно даже вообще индикации)

// TODO WiFi должен пытаться поключиться постоянно а не только при старте

// TODO повторное нажатие кнопки в режиме портала должно перезагружать плату?

void setup(void) {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  LED(1);
  Serial.begin(115200);
  delay(2000);  // ждём старта есп
  Serial.println();

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
  portalTick();
  LED(SP_started);
}
