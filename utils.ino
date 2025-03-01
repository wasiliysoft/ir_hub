// Функция для преобразования RAW данных в строку
String resultToRawArray(decode_results *results) {
  String rawData = "";
  for (uint16_t i = 1; i < results->rawlen; i++) {
    uint32_t usecs;
    for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX; usecs -= UINT16_MAX) {
      rawData += uint64ToString(UINT16_MAX);
      rawData += ",";
    }
    rawData += uint64ToString(usecs);
    if (i < results->rawlen - 1)
      rawData += ",";  // Добавляем запятую, кроме последнего элемента
  }
  return rawData;
}