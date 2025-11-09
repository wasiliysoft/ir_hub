#include "GirsClient.h"

GirsClient *GirsClient::girsInstance = nullptr;

IRAM_ATTR void GirsClient::handleInterrupt() {
  const unsigned long currentTime = micros();
  const unsigned long duration = currentTime - g_lastIsrTime;
  g_lastIsrTime = currentTime;
  g_lastIsrTimeMs = millis();

  // Простая логика в IRAM - только запись данных
  if (duration > 50 && g_pulseIndex < GIRS_BUFFER_SIZE) {
    g_durations[g_pulseIndex++] = duration;
  }
}

void IRAM_ATTR GirsClient::handleInterruptStatic() {
  if (girsInstance) {
    girsInstance->handleInterrupt();
  }
}

String GirsClient::girs_getNextToken(String &str) {
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

void GirsClient::girs_sendRaw(const uint16_t intro[], unsigned lengthIntro,
                              const uint16_t repeat[], unsigned lengthRepeat,
                              const uint16_t ending[], unsigned lengthEnding,
                              uint16_t frequency, unsigned times) {
  if (lengthIntro > 0U)
    irServer.sendRaw(intro, lengthIntro, hz2khz(frequency));
  if (lengthRepeat > 0U) {
    for (unsigned i = 0U; i < times - (lengthIntro > 0U); i++) {
      irServer.sendRaw(repeat, lengthRepeat, hz2khz(frequency));
    }
  }
  if (lengthEnding > 0U)
    irServer.sendRaw(ending, lengthEnding, hz2khz(frequency));
}

void GirsClient::girs_receive(Stream &stream) {
  // Сброс состояния перед приемом
  g_pulseIndex = 0;
  g_lastIsrTime = micros();
  g_lastIsrTimeMs = millis();

  const unsigned long startTime = millis();
  bool isReady = true;
  bool isTimeout = false;

  // Включение прерывания
  attachInterrupt(digitalPinToInterrupt(IR_RECV_PIN), handleInterruptStatic,
                  CHANGE);
  digitalWrite(LED_PIN, LOW);

  // Ждем данные
  while (isReady) {
    const unsigned long lastIsrEdge = millis() - g_lastIsrTimeMs;

    // Условия выхода из цикла приема
    if (g_pulseIndex == 0 && (millis() - startTime) >= 5000) {
      // Таймаут, если нет импульсов
      isReady = false;
      isTimeout = true;
    } else if (g_pulseIndex > 1 && lastIsrEdge > 1500L) {
      // Завершение, если пауза между импульсами большая
      isReady = false;
    } else if (g_pulseIndex >= GIRS_BUFFER_SIZE) {
      // Буфер заполнен
      isReady = false;
    }
    yield();
  }

  // Выключение прерывания и индикации
  digitalWrite(LED_PIN, HIGH);
  detachInterrupt(digitalPinToInterrupt(IR_RECV_PIN));

  // Отправка результатов
  if (isTimeout) {
    stream.println('.');
  } else {
    // Обработка и отправка принятых данных
    g_pulseIndex &= ~1; // Обеспечиваем четное количество
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

void GirsClient::girs_processCommand(const String &line, Stream &stream) {
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

    // Используем динамическое выделение для больших массивов
    uint16_t *intro = new uint16_t[introLength];
    uint16_t *repeat = new uint16_t[repeatLength];
    uint16_t *ending = new uint16_t[endingLength];

    for (unsigned i = 0; i < introLength; i++)
      intro[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();
    for (unsigned i = 0; i < repeatLength; i++)
      repeat[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();
    for (unsigned i = 0; i < endingLength; i++)
      ending[i] = (uint16_t)girs_getNextToken(cmdStr).toInt();

    girs_sendRaw(intro, introLength, repeat, repeatLength, ending, endingLength,
                 frequency, noSends);

    // Освобождаем память
    delete[] intro;
    delete[] repeat;
    delete[] ending;

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

GirsClient::GirsClient() { girsInstance = this; }

void GirsClient::begin(Stream **streamArray, uint8_t count) {
  pinMode(IR_RECV_PIN, INPUT_PULLUP);

  // Инициализация массива потоков
  if (streamArray && count > 0) {
    streams = streamArray;
    streamCount = count;
  }
}

void GirsClient::addStream(Stream *stream) {
  if (!streams) {
    // Если массив не инициализирован, создаем его
    streams = new Stream *[1];
    streams[0] = stream;
    streamCount = 1;
  } else {
    // Увеличиваем массив и добавляем новый поток
    Stream **newStreams = new Stream *[streamCount + 1];
    for (uint8_t i = 0; i < streamCount; i++) {
      newStreams[i] = streams[i];
    }
    newStreams[streamCount] = stream;
    delete[] streams;
    streams = newStreams;
    streamCount++;
  }
}

void GirsClient::update() {
  // Обработка всех потоков в массиве
  for (uint8_t i = 0; i < streamCount; i++) {
    if (streams[i] && streams[i]->available()) {
      String line = streams[i]->readStringUntil('\r');
      girs_processCommand(line, *streams[i]);
    }
    yield();
  }
}