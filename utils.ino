// Функция для преобразования RAW данных в строку
String resultToRawArray(decode_results* results) {
  String rawData = "";
  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t usecs;
    for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
      rawData += uint64ToString(UINT16_MAX);
      rawData += ",";
    }
    rawData += uint64ToString(usecs);
    if (i < results->rawlen - 1) rawData += ",";  // Добавляем запятую, кроме последнего элемента
  }
  return rawData;
}

void powerWatchDogTic() {
  static enum { IDLE, PULSE_LOW } state = IDLE;
  static uint32_t lastTime = 0;  // вызывается только при инициализации переменной

  uint32_t currentTime = millis();

  switch (state) {
  case IDLE:
    if (currentTime - lastTime >= 5000) {
      digitalWrite(powerWatchDogPin, LOW);
      lastTime = currentTime;
      state = PULSE_LOW;
    }
    break;

  case PULSE_LOW:
    if (currentTime - lastTime >= 50) {
      digitalWrite(powerWatchDogPin, HIGH);
      state = IDLE;
    }
    break;
  }
}