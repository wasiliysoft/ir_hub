#include "main.h"

bool startWiFi() {
  bool result = false;
  if (cfg.mode == WIFI_AP) result = setupAP();
  else result = setupLocal();
  return result;
}

bool setupAP() {
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  delay(100);
  Serial.println("Starting as AP...");
  Serial.printf("SSID %s pass %s\n", cfg.SSID, cfg.pass);
  if (cfg.pass[0] == NULL) {
    Serial.println("no password");
    WiFi.softAP(cfg.SSID);
  } else {
    WiFi.softAP(cfg.SSID, cfg.pass);
  }

  Serial.println("Setting AP Mode");
  Serial.print("AP home page: http:// ");
  Serial.println(WiFi.softAPIP());
  return true;
}

bool setupLocal() {
  if (cfg.SSID[0] == NULL && cfg.pass[0] == NULL) {
    Serial.println("WiFi not configured");
    portalStart();
    return true;
  } else {
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    Serial.println("Connecting to AP...");
    Serial.printf("SSID %s pass %s\n", cfg.SSID, cfg.pass);
    uint32_t tmr = millis();
    WiFi.begin(cfg.SSID, cfg.pass);

    while (WiFi.status() != WL_CONNECTED && millis() - tmr < 30000) {
      Serial.print(".");
      portalBtnTick();
      if (SP_started) return true;
      blinkConnectionLed();
      yield();
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Connected, home page: http://");
      Serial.println(WiFi.localIP());
      return true;
    } else {
      Serial.println("Connection failed!");
      delay(200);
      return false;
    }
  }
}

void blinkConnectionLed() {
  LED(1);
  delay(200);
  LED(0);
  delay(200);
}