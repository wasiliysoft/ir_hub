#ifndef CONFIG_PIN_H
#define CONFIG_PIN_H

#ifdef ESP8266
// Пин для ИК светодиода (D1 на Wemos D1 Mini соответствует GPIO 5)
#define IR_LED_PIN D1
// Пин, к которому подключен ИК-приемник
#define IR_RECV_PIN D2
// Пин для кнопки "Сброс и приготовиться"
#define READY_TO_RECEIVE_BTN_PIN D3
// Пин для светодиода (D4 на Wemos D1 Mini соответствует GPIO 2)
#define LED_PIN D4
// Пин для поддержания ВКЛ состояния на модуле питания
#define POWER_WATCH_DOG_PIN D5

#define BT_RX_PIN D6  // подключен к TX HC-06
#define BT_TX_PIN D7  // подключен к RX HC-06

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
// Пин для ИК светодиода
#define IR_LED_PIN GPIO_NUM_8
// Пин, к которому подключен ИК-приемник
#define IR_RECV_PIN GPIO_NUM_3
// Пин для кнопки "Сброс и приготовиться"
#define READY_TO_RECEIVE_BTN_PIN GPIO_NUM_4
// Пин для светодиода
#define LED_PIN GPIO_NUM_8
// Пин для поддержания ВКЛ состояния на модуле питания
#define POWER_WATCH_DOG_PIN GPIO_NUM_5

#elif defined(CONFIG_IDF_TARGET_ESP32)
// Без каких-либо ограничений в качестве входов и выходов в ESP32 DevKit V1 можно использовать следующие 12 выводов GPIO:
// 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33.
// https://myrobot.ru/wiki/index.php?n=Experiences.Esp32Pinout

// Пин для светодиода
#define LED_PIN GPIO_NUM_2
// Пин для ИК светодиода
#define IR_LED_PIN GPIO_NUM_18
// Пин, к которому подключен ИК-приемник
#define IR_RECV_PIN GPIO_NUM_19
// Пин для кнопки "Сброс и приготовиться"
#define READY_TO_RECEIVE_BTN_PIN GPIO_NUM_21
// Пин для поддержания ВКЛ состояния на модуле питания
#define POWER_WATCH_DOG_PIN GPIO_NUM_23
#endif

#endif