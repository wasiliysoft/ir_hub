#include "main.h"

void portalStart() {
  Serial.println();
  Serial.println("Run captive portal");
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(500);
  IPAddress apIP(SP_AP_IP);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(SP_AP_NAME);
  delay(500);
  dnsServer.start(53, "*", apIP);
  restartHTTP();
  restartUDP();
  SP_started = true;
  yield();
}

void portalTick() {
  if (SP_started) {
    dnsServer.processNextRequest();
    yield();
  }
}

void portalBtnTick() {
  if (digitalRead(BTN_PIN) == LOW) {
    LED(1);
    portalStart();
    delay(1000);
  }
}


