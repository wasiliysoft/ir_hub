#include "main.h"

const String postForms =
    "<html>\
    <head>\
      <title>ESP8266 Web Server POST handling</title>\
    </head>\
      <body>\
        <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/sendIr/\">\
        <input type=\"text\" name=\"freq\" value=\"freq\"><br>\
        <input type=\"text\" name=\"patt\" value=\"patt\"><br>\
        <input type=\"submit\" value=\"Submit\">\
        </form>\
      </body>\
    </html>";

const char SP_connect_page[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
<style type="text/css">
input {
padding: 1ex;
margin: 1ex;
}
input[type="text"] {
width: 80%;
}
input[type="submit"] {
width: 80%;
/* height: 6ex; */
}
</style>
<center>
<h3>WiFi settings</h3>
<form action="/save" method="POST">
<input type="radio" id="r_client" name="mode" value="client" checked />
<label for="r_client">Client</label><br>
<input type="radio" id="r_ap" name="mode" value="ap" />
<label for="r_ap">Access Point</label><br><br>
<input type="text" name="ssid" placeholder="SSID"><br>
<input type="text" name="pass" placeholder="Pass"><br>
<input type="submit" value="SAVE">
</form>
<form action="/exit" method="POST">
<input type="submit" value="EXIT">
</form>
</center>
</body>
</html>
)rawliteral";

void restartHTTP() {
  server.stop();
  server.on("/formIr/", IR_handleForm);
  server.on("/sendIr/", HTTP_POST, IR_handleSend);
  server.on("/save", HTTP_POST, SP_handleSave);
  server.on("/exit", HTTP_POST, SP_handleExit);
  server.on("/", SP_handleConnect);
  server.onNotFound(SP_handleConnect);
  server.begin();
  Serial.printf("HTTP server started on port %d\n", HTTP_PORT);
}

void SP_handleConnect() { server.send(200, "text/html", SP_connect_page); }

void IR_handleForm() { server.send(200, "text/html", postForms); }

void IR_handleSend() {
  // LED(1);
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    String message = "";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += "" + server.argName(i) + ": " + server.arg(i) + "\n";
      yield();
    }
    float freq = server.arg("freq").toInt() / 1000;
    char separator = ',';
    uint16_t pulses = getSeparatorCount(server.arg("patt"), separator) + 1;
    uint16_t buf[pulses];
    parsePatt(server.arg("patt"), separator, buf);
    irsend.sendRaw(buf, sizeof(buf) / sizeof(buf[0]), freq);
    yield();
    server.send(200, "text/plain", message);

    Serial.print("freq ");
    Serial.println(freq);
    Serial.print("patt ");
    for (int i = 0; i < pulses; i++) {
      Serial.print(buf[i]);
      Serial.print(",");
      yield();
    }
    Serial.println();
    yield();
  }
  LED(0);
}

void SP_handleSave() {
  if (server.arg("mode") == "ap") {
    portalCfg.mode = WIFI_AP;
  } else {
    portalCfg.mode = WIFI_STA;
  }
  strcpy(portalCfg.SSID, server.arg("ssid").c_str());
  strcpy(portalCfg.pass, server.arg("pass").c_str());
  _SP_status = 1;
}

void SP_handleExit() { _SP_status = 4; }

void parsePatt(const String& data, const char separator, uint16_t* buf) {
  yield();
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;
  int pulse = 0;
  for (int i = 0; i <= maxIndex; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
      buf[pulse] = data.substring(strIndex[0], strIndex[1]).toInt();
      pulse++;
      yield();
    }
    yield();
  }
  yield();
}

uint16_t getSeparatorCount(const String& data, const char separator) {
  int found = 0;
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex; i++) {
    if (data.charAt(i) == separator) {
      found++;
    }
    yield();
  }
  return found;
}