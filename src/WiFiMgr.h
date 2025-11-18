#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#else
#include <ESPmDNS.h>
#include <WiFi.h>
#endif

#include <DNSServer.h>

extern ConfigMgr config;

class WiFiMgr {
  DNSServer dnsServer;

public:
  void begin() {
#if defined(CONFIG_IDF_TARGET_ESP32C3)
    // Установка мощности передатчика WiFi
    // https://github.com/sigmdel/supermini_esp32c3_sketches?tab=readme-ov-file#05_wifi_tx_power
    WiFi.setTxPower(WIFI_POWER_11dBm);
#endif
    if (config.isAPMode()) {
      startAPMode();
    } else {
      startClientMode();
    }

    // Инициализация mDNS
    if (!MDNS.begin(HOSTNAME)) {
      Serial.println("Ошибка настройки mDNS!");
    } else {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS запущен, имя хоста: http://" + String(HOSTNAME) + ".local");
    }
    Serial.println("WiFiMgr started");
  }

  void update() {
    // Обработка DNS-запросов в режиме точки доступа
    if (config.isAPMode()) {
      dnsServer.processNextRequest();
    }
#ifdef ESP8266
    MDNS.update();
#endif
  }

private:
  void startAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.getSsid(), config.getPassword());
    Serial.println("Точка доступа запущена");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    // Запуск DNS-сервера для перенаправления всех запросов на IP ESP8266
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.println("DNS сервер запущен");
  }

  void startClientMode() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.getSsid(), config.getPassword());
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_PIN, LOW);
      delay(400);
      digitalWrite(LED_PIN, HIGH);
      delay(400);
      Serial.print(".");
      if (millis() > 10000) {
        Serial.println("Не удается подключиться к сети, включаем точку доступа");
        config.setDefaultSettings();
        startAPMode();
        return;
      }
    }
    Serial.println("");
    Serial.println("WiFi подключен");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
};

#endif  // WIFI_MANAGER_H