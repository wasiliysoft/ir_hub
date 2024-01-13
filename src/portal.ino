#include "main.h"

void portalStart() {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(500);
  IPAddress apIP(SP_AP_IP);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, subnet);
  WiFi.softAP(SP_AP_NAME);
  dnsServer.start(53, "*", apIP);
  SP_started = true;
  _SP_status = 0;
  Serial.println();
  Serial.println("Run captive portal");
  yield();
}

void portalStop() {
  WiFi.softAPdisconnect();
  // server.stop();
  dnsServer.stop();
  SP_started = false;
  yield();
}

bool portalTick() {
  if (SP_started) {
    dnsServer.processNextRequest();
    server.handleClient();
    yield();
    if (_SP_status) {
      portalStop();
      return 1;
    }
  }
  return 0;
}

// void portalRun(uint32_t prd) {
//   uint32_t tmr = millis();
//   portalStart();
//   while (!portalTick()) {
//     if (millis() - tmr > prd) {
//       _SP_status = 5;
//       portalStop();
//       break;
//     }
//     yield();
//   }
// }

byte portalStatus() { return _SP_status; }

void portalBtnTick() {
  if (digitalRead(BTN_PIN) == LOW) {
    portalStart();
    LED(1);
    delay(1000);
  }

  if (portalTick()) {
    // сработает однократно при действии
    // точка будет автоматически выключена
    Serial.printf("Portal result status %d\n", portalStatus());
    if (portalStatus() == SP_SUBMIT) {
      Serial.println(portalCfg.mode);
      Serial.println(portalCfg.SSID);
      Serial.println(portalCfg.pass);

      cfg.mode = portalCfg.mode;
      strcpy(cfg.SSID, portalCfg.SSID);
      strcpy(cfg.pass, portalCfg.pass);

      EE_save();
      delay(2000);
    }
    ESP.reset();
  }
  yield();
}
