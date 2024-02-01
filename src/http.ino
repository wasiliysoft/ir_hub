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
    <!-- <base href="http://192.168.0.176/" /> TODO Remove in prod -->
    <style type="text/css">
        input {
            padding: 1ex;
            margin: 1ex;
        }

        input[type="text"] {
            width: 80%;
        }

        button {
            width: 80%;
            padding: 1ex;
            margin: 1ex;
            /* height: 6ex; */
        }
    </style>
</head>

<body>
    <center>
        <h3>WiFi Config</h3>
        <hr>
        <button value="SCAN WIFI" onclick="window.location.reload();">SCAN WIFI</button>
        <hr>
        <div id="found_networks">
        </div>
        <hr>
        <form action="/sp/save" method="POST">
            <input id="ssid" type="text" name="ssid" placeholder="SSID"><br>
            <input type="text" name="pass" placeholder="password"><br>
            <input type="radio" id="r_client" name="mode" value="client" checked />
            <label for="r_client">Client</label>&nbsp;&nbsp;&nbsp;&nbsp;
            <input type="radio" id="r_ap" name="mode" value="ap" />
            <label for="r_ap">Access Point</label>
            <hr>
            <button>SAVE</button>
        </form>
        <button onclick="window.location.href='/sp/exit';">EXIT</button>
    </center>
    <script>
        scanWiFi();
        function scanWiFi() {
            // let scanResult = "WasiliySoft\nQBR-2041WW_1698\nTP-Link_E11A\nBk\n"; // TODO Remove in prod
            // createFoundNetworks(scanResult); // TODO Remove in prod
            fetch("/sp/scan_networks")
                .then((response) => response.text())
                .then((scanResult) => {
                    // console.log(scanResult);
                    createFoundNetworks(scanResult);
                }
                );
        }

        function createFoundNetworks(scanResult) {
            var parent = document.getElementById('found_networks');
            parent.innerHTML = "";
            scanResult.split("\n").forEach(function (item) {
                if (item != "") {
                    console.log(item);
                    var el = createNetworkElement(item);
                    parent.appendChild(el);
                }
            });
        }
        function createNetworkElement(networkName) {
            var el = document.createElement("button");

            el.innerText = networkName;
            el.onclick = function () {
                pasteSSID(networkName);
                // alert(networkName);
            };
            return el;
        }

        function pasteSSID(networkName) {
            document.getElementById('ssid').value = networkName;
        }
    </script>
</body>

</html>
)rawliteral";

void restartHTTP() {
  server.stop();
  server.on("/formIr", IR_handleForm);
  server.on("/sendIr/", HTTP_POST, IR_handleSend);  // deprecated
  server.on("/sendIr", HTTP_POST, IR_handleSend);
  server.on("/sp/save", HTTP_POST, SP_handleSave);
  server.on("/sp/scan_networks", SP_handleScan);
  server.on("/sp/exit", SP_handleExit);
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.printf("HTTP server started on port %d\n", HTTP_PORT);
}

void handleRoot() { SP_handleConnect(); }

void handleNotFound() {
  Serial.println("handleNotFound");
  handleRoot();
}

void SP_handleConnect() { server.send(200, "text/html", SP_connect_page); }

void IR_handleForm() { server.send(200, "text/html", postForms); }

void IR_handleSend() {
  LED(1);
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
  LED(0);
  yield();
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
  server.send(200, "text/html", "ok");
}

void SP_handleExit() {
  _SP_status = 4;
  handleRoot();
}

void SP_handleScan() {
  Serial.println("scan begin");
  int numberOfNetworks = WiFi.scanNetworks(/*async=*/false, /*hidden=*/false);
  String result = "";
  for (int i = 0; i < numberOfNetworks; i++) {
    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));
    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI(i));
    Serial.println("-----------------------");
    yield();
    result += WiFi.SSID(i) + "\n";
    yield();
  }
  server.send(200, "text/html", result);
  Serial.println("scan complete");
}

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