#ifndef IRHUB_CONFIG_H
#define IRHUB_CONFIG_H
#include <Arduino.h>
#include <EEPROM.h>

// #define DEBUG     // Раскомментировать для включения отладочных сообщений
// #define BT_HC06   // Раскомментировать для включения BT_HC06 на плате esp8266

#include "config_pin.h"

#define FIRMWARE_VER "v2.7.0 (" __DATE__ " " __TIME__ ")"
#define HOSTNAME "irhub"

#define UDP_PORT 55531  // Порт для широковещательного UDP

#define INIT_ADDR 1023  // номер ячейки для хранения клоюча первого запуска
#define INIT_KEY 53     // ключ первого запуска. 0-254, на выбор

// Макросы
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

static const String waitText = "Ожидание сигнала..";
struct IRData {
  String hexcode;
  String protocol;
  String raw;

  IRData() : hexcode(waitText), protocol(waitText), raw(waitText) {}
};

// Класс для управления настройками
class ConfigMgr {
public:
  // Инициализация EEPROM и загрузка настроек
  void begin() {
    EEPROM.begin(4096);  // https://alexgyver.ru/lessons/eeprom/#3-toc-title
    load();
    Serial.println("ConfigMgr started");
  }

  char* getSsid() { return settings.ssid; }
  char* getPassword() { return settings.password; }
  bool isAPMode() { return settings.isAPMode; }

  void setWifiSettings(const char* ssid, const char* password, const bool isAPMode) {
    if (ssid != nullptr && password != nullptr) {
      strlcpy(settings.ssid, ssid, sizeof(settings.ssid));
      strlcpy(settings.password, password, sizeof(settings.password));
      settings.isAPMode = isAPMode;
    }
  }

  // Сохраняет настройки в EEPROM
  void commit() {
    EEPROM.put(0, settings);
    EEPROM.commit();
    Serial.println("EEPROM commit complete");
  }

  void setDefaultSettings() {
    strlcpy(settings.ssid, getUniqueHostname(), sizeof(settings.ssid));
    strcpy(settings.password, "");
    settings.isAPMode = true;
  }


  char* getUniqueHostname() {
#ifdef ESP8266
    uint16_t id = (uint16_t)(ESP.getChipId());
#else
    uint16_t id = (uint16_t)(ESP.getEfuseMac());
#endif
    static char buffer[sizeof(HOSTNAME) + 1 + 4];
    sprintf(buffer, HOSTNAME "_%04x", id);
    return buffer;
  }

private:
  struct Settings {
    char ssid[32];
    char password[64];
    bool isAPMode;
  } settings;

  // Загружает настройки из EEPROM и выполняет валидацию
  void load() {
    if (EEPROM.read(INIT_ADDR) != INIT_KEY) {  // первый запуск
      delay(1000);
      Serial.println("Первый запуск");
      Serial.println("Первый запуск");
      Serial.println("Первый запуск");
      EEPROM.write(INIT_ADDR, INIT_KEY);  // записали ключ
      setDefaultSettings();
      commit();
    } else {
      EEPROM.get(0, settings);
    }
  }
};

#endif