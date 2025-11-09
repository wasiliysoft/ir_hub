#ifndef GIRSCLIENT_H
#define GIRSCLIENT_H

#ifndef IRHUB_CONFIG_H
#include "config.h"
#endif

#ifndef IR_SERVER_H
#include "IrServer.h"
#endif

extern IrServer irServer;

class GirsClient {
private:
  static const unsigned long DUMMYENDING = 40000U;
  static const uint16_t GIRS_BUFFER_SIZE = 512;
  volatile unsigned long g_lastIsrTime = 0;
  volatile unsigned long g_lastIsrTimeMs = 0;
  volatile unsigned long g_durations[GIRS_BUFFER_SIZE];
  volatile uint16_t g_pulseIndex = 0;

  static GirsClient *girsInstance;
  
  // Массив потоков для обработки
  Stream** streams = nullptr;
  uint8_t streamCount = 0;

  // Обработчик прерывания - должен быть в IRAM
  IRAM_ATTR void handleInterrupt();

  // Статический метод-обертка - должен быть в IRAM
  static void IRAM_ATTR handleInterruptStatic();

  // Преобразование Гц в кГц
  static inline unsigned hz2khz(uint16_t hz) { return (hz + 500) / 1000; }

  // Округление до ближайшего 32
  static inline unsigned roundToNearest32(unsigned long value) {
    return (value + 16) & 0xFFFFFFE0;
  }

  // Получение следующего токена из строки
  String girs_getNextToken(String &str);

  // Отправка ИК-сигнала
  void girs_sendRaw(const uint16_t intro[], unsigned lengthIntro,
                    const uint16_t repeat[], unsigned lengthRepeat,
                    const uint16_t ending[], unsigned lengthEnding,
                    uint16_t frequency, unsigned times);

  // Прием ИК-сигнала
  void girs_receive(Stream &stream);

  // Обработка команд
  void girs_processCommand(const String &line, Stream &stream);

public:
  GirsClient();
  
  // Инициализация с массивом потоков
  void begin(Stream** streamArray = nullptr, uint8_t count = 0);
  
  // Добавление потока динамически
  void addStream(Stream* stream);
  
  void update();
};

#endif // GIRSCLIENT_H