#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#ifndef UDP_SERVER_H
#include "UDPServer.h"
#endif
#ifndef IR_SERVER_H
#include "IrServer.h"
#endif

#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h> // https://randomnerdtutorials.com/arduino-ide-2-install-esp8266-littlefs/#installing-windows

extern Config config;
extern UDPServer udp;
extern void readyToReceive();

class WebUI {
private:
  ESP8266WebServer server;
  ESP8266HTTPUpdateServer httpUpdater;

public:
  WebUI() : server(80) {}

  void begin() {
    LittleFS.begin();

    // Настройка маршрутов веб-сервера
    httpUpdater.setup(&server); // OTA url /update
    server.onNotFound([this]() {
      if (!LittleFS.exists("/index.html")) {
        Serial.println("Файловая система не найдена!");
        server.sendHeader("Location", "/update");
      } else {
        server.sendHeader("Location", "/");
      }
      server.send(302, "text/plain", "Redirecting");
    });
    server.on("/api/v1/last-received-data", HTTP_GET,
              [this]() { this->handleAPI_last_received_data(); });
    server.on("/api/v1/scan-network", HTTP_GET,
              [this]() { this->handleAPI_scan_network(); });
    server.on("/api/v1/config-read", HTTP_GET,
              [this] { this->handleAPI_config_read(); });
    server.on("/api/v1/config-write", HTTP_POST,
              [this] { this->handleAPI_config_write(); });
    server.on("/api/v1/config-erase", HTTP_GET,
              [this] { this->handleAPI_config_erase(); });
    server.on("/reset", HTTP_GET, [this] { this->handleReset(); });
    server.on("/sendIr/", HTTP_POST, [this] { this->handleSendRaw(); });
    server.serveStatic("/", LittleFS, "/",
                       "max-age=86400"); // 1 сутки = 24 * 3600 = 86400
    server.begin();                      // Запуск веб-сервера
    Serial.println("Web-server started");
  }
  void update() { server.handleClient(); }

private:
  void handleAPI_last_received_data() {
    JsonDocument doc;
    doc["protocol"] = irServer.getLastIRData().protocol;
    doc["code"] = irServer.getLastIRData().hexcode;
    doc["raw"] = irServer.getLastIRData().raw;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  }

  void handleAPI_config_read() {
    JsonDocument doc;
    doc["w_ssid"] = config.settings.ssid;
    doc["w_pass"] = config.settings.password;
    doc["w_ap"] = config.settings.isAPMode;
    doc["fw_ver_name"] = FIRMWARE_VER;
    // Добавляем новые поля
    doc["local_ip"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["hostname"] = WiFi.hostname();
    // doc["subnet_mask"] = WiFi.subnetMask().toString();
    // doc["gateway_ip"] = WiFi.gatewayIP().toString();
    // doc["dns_ip"] = WiFi.dnsIP().toString();
    doc["rssi"] = WiFi.RSSI();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  }

  void handleAPI_config_write() {
    if (server.method() == HTTP_POST) {
      config.settings.isAPMode = server.arg("mode").toInt() == 1;
      strncpy(config.settings.ssid, server.arg("ssid").c_str(),
              sizeof(config.settings.ssid));
      strncpy(config.settings.password, server.arg("password").c_str(),
              sizeof(config.settings.password));
      config.commit();

      JsonDocument doc;
      doc["status"] = "ok";

      String response;
      serializeJson(doc, response);
      server.send(200, "application/json", response);
      delay(1000);
      ESP.restart();
    }
  }

  void handleAPI_config_erase() {
    JsonDocument doc;
    doc["status"] = "ok";

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);

    Serial.println("Сброс параметров подключения WiFi");
    config.setDefaultSettings();
    config.commit();
    Serial.println("Перезагрузка...");
    delay(2000);
    ESP.reset();
  }

  void handleAPI_scan_network() {
    JsonDocument doc;

    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
      doc[i] = WiFi.SSID(i);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  }

  // Обработка кнопки "Сбросить и приготовиться"
  void handleReset() {
    readyToReceive();
    server.sendHeader("Location", "/");
    server.send(303);
  }

  void handleSendRaw() {
    digitalWrite(LED_PIN, LOW);
    uint16_t hz = server.arg("freq").toInt();
    String pattern = server.arg("patt");

    int pulses = 0;
    int startIndex = 0;

    uint16_t irSendBuf[255]; // буфер для хранения RAW шаблона команды

    for (unsigned int i = 0; i <= pattern.length(); i++) {
      if (i == pattern.length() || pattern.charAt(i) == ',') {
        irSendBuf[pulses++] = pattern.substring(startIndex, i).toInt();
        startIndex = i + 1;
      }
      yield();
    }

    // отправляем на все узлы в сети
    // TODO Добавить в WEB интерфейс выбор отправки по UDP или только локально
    udp.sendUDPRawIR(irSendBuf, pulses, hz);
    yield();
    // отправляем команду на ИК диод
    irServer.sendRaw(irSendBuf, pulses, hz);
    yield();
    server.send(200, "text/plain", "success");
    digitalWrite(LED_PIN, HIGH);
  }
};

#endif