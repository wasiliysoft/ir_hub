#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

extern ConfigMgr config;

class WiFiMgr {
  DNSServer dnsServer;

public:
  void begin() {
    if (config.settings.isAPMode) {
      startAPMode();
    } else {
      startClientMode();
    }

    // Инициализация mDNS
    if (!MDNS.begin(HOSTNAME)) {
      Serial.println("Ошибка настройки mDNS!");
    } else {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS запущен, имя хоста: http://" + String(HOSTNAME) +
                     ".local");
    }
  }

  void uopdate() {
    // Обработка DNS-запросов в режиме точки доступа
    if (config.settings.isAPMode) {
      dnsServer.processNextRequest();
    }
    // Обновление mDNS
    MDNS.update();
  }

private:
  void startAPMode() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(config.settings.ssid, config.settings.password);
    Serial.println("Точка доступа запущена");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    // Запуск DNS-сервера для перенаправления всех запросов на IP ESP8266
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.println("DNS сервер запущен");
  }

  void startClientMode() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.settings.ssid, config.settings.password);
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_PIN, LOW);
      delay(400);
      digitalWrite(LED_PIN, HIGH);
      delay(400);
      Serial.print(".");
      if (millis() > 10000) {
        Serial.println(
            "Не удается подключиться к сети, включаем точку доступа");
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

#endif // WIFI_MANAGER_H