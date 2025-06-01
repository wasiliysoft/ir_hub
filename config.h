#ifndef IRHUB_CONFIG_H
#define IRHUB_CONFIG_H
#include <Arduino.h>

#define BT_HC06

#define FIRMWARE_VER "v2.6.0 (2025.05.29)"
#define SSID_DEFAULT "AutoConnectAP"
#define HOSTNAME "irhub"

#define UDP_PORT 55531  // Порт для широковещательного UDP

#define IR_LED_PIN D1                // Пин для ИК светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define IR_RECV_PIN D2               // Пин, к которому подключен ИК-приемник
#define READY_TO_RECEIVE_BTN_PIN D3  // Пин для кнопки "Сброс и приготовиться"
#define LED_PIN D4                   // Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)
#define POWER_WATCH_DOG_PIN D5       // Пин для поддержания ВКЛ состояния на модуле питания
#define BT_RX_PIN D6                 // подключен к TX HC-06
#define BT_TX_PIN D7                 // подключен к RX HC-06

// Макросы
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif
#endif