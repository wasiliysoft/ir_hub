#include "GirsClient.h"

#ifdef BT_HC06
#include <SoftwareSerial.h>
SoftwareSerial BTserial(BT_RX_PIN, BT_TX_PIN);
#endif



// Глобальные переменные для обработчика прерывания
static const unsigned long DUMMYENDING = 40000U;
static const uint16_t GIRS_BUFFER_SIZE = 512;
volatile unsigned long g_lastIsrTime = 0;
volatile unsigned long g_lastIsrTimeMs = 0;
volatile unsigned long g_durations[GIRS_BUFFER_SIZE];
volatile uint16_t g_pulseIndex = 0;

// Обработчик прерывания
IRAM_ATTR void handleInterrupt() {
  const unsigned long currentTime = micros();
  const unsigned long duration = currentTime - g_lastIsrTime;
  g_lastIsrTime = currentTime;
  g_lastIsrTimeMs = millis();

  if (duration > 50 && g_pulseIndex < GIRS_BUFFER_SIZE) {
    g_durations[g_pulseIndex++] = duration;
  }
}
// Инициализация
void girs_begin() {
  pinMode(IR_RECV_PIN, INPUT_PULLUP);
#ifdef BT_HC06
  BTserial.begin(9600);  // Стандартная скорость HC-06
#endif
}

// Основной цикл обработки команд
void girs_tic() {
  // Обработка последовательного порта
  if (Serial.available()) {
    String line = Serial.readStringUntil('\r');
    girs_processCommand(line, Serial);
  }
  yield();
#ifdef BT_HC06
  // Обработка Bluetooth
  if (BTserial.available()) {
    String line = BTserial.readStringUntil('\r');
    girs_processCommand(line, BTserial);
  }
  yield();
#endif
}



// Преобразование Гц в кГц
static inline unsigned hz2khz(uint16_t hz) {
  return (hz + 500) / 1000;
}

// Округление до ближайшего 32
static inline unsigned roundToNearest32(unsigned long value) {
  return (value + 16) & 0xFFFFFFE0;
}

// Получение следующего токена из строки
String girs_getNextToken(String& str) {
  str.trim();
  int spacePos = str.indexOf(' ');
  if (spacePos == -1) {
    String token = str;
    str = "";
    return token;
  }
  String token = str.substring(0, spacePos);
  yield();
  str = str.substring(spacePos + 1);
  yield();
  return token;
}

// Отправка ИК-сигнала
void girs_sendRaw(const uint16_t intro[], unsigned lengthIntro, const uint16_t repeat[], unsigned lengthRepeat, const uint16_t ending[], unsigned lengthEnding, uint16_t frequency, unsigned times) {
  if (lengthIntro > 0U) irsend.sendRaw(intro, lengthIntro, hz2khz(frequency));
  if (lengthRepeat > 0U) {
    for (unsigned i = 0U; i < times - (lengthIntro > 0U); i++) {
      irsend.sendRaw(repeat, lengthRepeat, hz2khz(frequency));
    }
  }
  if (lengthEnding > 0U) irsend.sendRaw(ending, lengthEnding, hz2khz(frequency));
}

// Прием ИК-сигнала
void girs_receive(Stream& stream) {
  g_pulseIndex = 0;
  const unsigned long startTime = millis();
  bool isReady = true;
  g_lastIsrTime = micros();
  g_lastIsrTimeMs = millis();
  attachInterrupt(digitalPinToInterrupt(IR_RECV_PIN), handleInterrupt, CHANGE);
  bool isTimeout = false;
  digitalWrite(LED_PIN, LOW);

  while (isReady) {
    const unsigned long lastIsrEdge = millis() - g_lastIsrTimeMs;
    if (g_pulseIndex == 0 && (millis() - startTime) >= 5000) {
      isReady = false;
      isTimeout = true;
    } else if (g_pulseIndex > 1 && lastIsrEdge > 1500L) {
      isReady = false;
    } else if (g_pulseIndex >= GIRS_BUFFER_SIZE) {
      isReady = false;
    }
    yield();
  }

  digitalWrite(LED_PIN, HIGH);
  detachInterrupt(IR_RECV_PIN);

  if (isTimeout) {
    stream.println('.');
  } else {
    g_pulseIndex &= ~1;
    for (uint16_t i = 1; i < g_pulseIndex; i++) {
      unsigned long duration = g_durations[i];
#ifdef ROUND_TO_NEAREST_32
      duration = roundToNearest32(duration);
#endif
      stream.write(i & 1 ? '+' : '-');
      stream.print(duration);
      stream.print(" ");
      yield();
    }
    stream.print('-');
    stream.println(DUMMYENDING);
  }
}

// Обработка команд
void girs_processCommand(const String& line, Stream& stream) {
  String cmdStr = line;
  String cmd = girs_getNextToken(cmdStr);

  if (cmd.length() == 0) {
    stream.println(F("OK"));
    return;
  }

  switch (cmd[0]) {
  case 'm':
    stream.println(F("base transmit receive"));
    break;
  case 'r':
    girs_receive(stream);
    break;
  case 's': {
    unsigned noSends = (unsigned)girs_getNextToken(cmdStr).toInt();
    uint16_t frequency = (uint16_t)girs_getNextToken(cmdStr).toInt();
    unsigned introLength = (unsigned)girs_getNextToken(cmdStr).toInt();
    unsigned repeatLength = (unsigned)girs_getNextToken(cmdStr).toInt();
    unsigned endingLength = (unsigned)girs_getNextToken(cmdStr).toInt();

    uint16_t intro[introLength];
    uint16_t repeat[repeatLength];
    uint16_t ending[endingLength];

    for (unsigned i = 0; i < introLength; i++)
      intro[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();
    for (unsigned i = 0; i < repeatLength; i++)
      repeat[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();
    for (unsigned i = 0; i < endingLength; i++)
      ending[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();

    girs_sendRaw(intro, introLength, repeat, repeatLength, ending, endingLength, frequency, noSends);
    yield();
    stream.println(F("OK"));
    yield();
    break;
  }
  case 'v':
    stream.println(F(HOSTNAME " " FIRMWARE_VER));
    break;
  default:
    stream.println(F("ERROR"));
  }
}