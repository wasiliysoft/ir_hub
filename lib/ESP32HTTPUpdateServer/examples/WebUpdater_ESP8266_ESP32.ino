/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" ESP-WebUpdate.local/update
*/

#ifdef ESP8266
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPUpdateServer.h>
  #include <ESP8266mDNS.h>
  #include <ESP8266WiFi.h>

  using WM_httpUpdater = ESP8266HTTPUpdateServer;
  using WM_WebServer = ESP8266WebServer;
  
#elif ESP32
  #include <WiFi.h> 
  #include <Update.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
  #include <ESP32HTTPUpdateServer.h>

  using WM_httpUpdater = esp32httpupdateserver_ns::ESP32HTTPUpdateServer;
  using WM_WebServer = WebServer;
#endif

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char* host = "ESP-WebUpdate";
const char* ssid = STASSID;
const char* password = STAPSK;

WM_WebServer httpServer(80);
WM_httpUpdater httpUpdater;

void setup(void) {

  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void loop(void) {
  httpServer.handleClient();
  
  #ifdef ESP8266
    MDNS.update();
  #endif
}
