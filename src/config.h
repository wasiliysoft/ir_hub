#ifndef IRHUB_CONFIG_H
#define IRHUB_CONFIG_H
#include <Arduino.h>
#include <EEPROM.h>

// #define BT_HC06

#define FIRMWARE_VER "v2.6.0 (2025.05.29)"
#define SSID_DEFAULT "AutoConnectAP"
#define HOSTNAME "irhub"

#define UDP_PORT 55531 // Порт для широковещательного UDP

#define IR_LED_PIN                                                             \
  1 // Пин для ИК светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define IR_RECV_PIN 2              // Пин, к которому подключен ИК-приемник
#define READY_TO_RECEIVE_BTN_PIN 3 // Пин для кнопки "Сброс и приготовиться"
#define LED_PIN                                                                \
  4 // Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)
#define POWER_WATCH_DOG_PIN                                                    \
  5                 // Пин для поддержания ВКЛ состояния на модуле питания
#define BT_RX_PIN D6 // подключен к TX HC-06
#define BT_TX_PIN D7 // подключен к RX HC-06

#define INIT_ADDR 1023 // номер ячейки для хранения клоюча первого запуска
#define INIT_KEY 53    // ключ первого запуска. 0-254, на выбор

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
    // EEPROM.begin(sizeof(Settings));
    EEPROM.begin(1023);
    load();
    Serial.println("ConfigMgr started");
  }
  // Структура для хранения настроек
  struct Settings {
    char ssid[32];
    char password[64];
    bool isAPMode;
  } settings;

  // Сохраняет настройки в EEPROM
  void commit() {
    EEPROM.put(0, settings);
    EEPROM.commit();
    Serial.println("EEPROM commit complete");
  }

  void setDefaultSettings() {
    strcpy(settings.ssid, SSID_DEFAULT);
    strcpy(settings.password, "");
    settings.isAPMode = true;
  }

private:
  // Загружает настройки из EEPROM и выполняет валидацию
  void load() {
    if (EEPROM.read(INIT_ADDR) != INIT_KEY) { // первый запуск
      delay(5000);
      Serial.println("Первый запуск");
      Serial.println("Первый запуск");
      Serial.println("Первый запуск");
      EEPROM.write(INIT_ADDR, INIT_KEY); // записали ключ
      setDefaultSettings();
      commit();
    } else {
      EEPROM.get(0, settings);
    }
  }
};

#endif