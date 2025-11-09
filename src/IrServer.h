#ifndef IR_SERVER_H
#define IR_SERVER_H
#include <Arduino.h>
#include <IRrecv.h>
#include <IRremoteESP8266.h> //2.8.6
#include <IRsend.h>
#include <IRutils.h>

#ifndef WEB_SOCKET_MGR_H
#include "WebSocketMgr.h"
#endif

extern WebSocketMgr webSocketMgr;

class IrServer {

private:
  IRData lastIRData;

  IRrecv recv = IRrecv(IR_RECV_PIN);
  IRsend send = IRsend(IR_LED_PIN);

  decode_results results; // Буфер для хранения полученных ИК данных

  // Переменные для хранения последней полученной ИК-команды

  bool isWaitingForIR = false; // Флаг для управления состоянием ИК-приемника

  // Функция для преобразования RAW данных в строку
  String resultToRawArray(decode_results *results) {
    String rawData = "";
    for (uint16_t i = 1; i < results->rawlen; i++) {
      uint32_t usecs;
      for (usecs = results->rawbuf[i] * kRawTick; usecs > UINT16_MAX;
           usecs -= UINT16_MAX) {
        rawData += uint64ToString(UINT16_MAX);
        rawData += ",";
      }
      rawData += uint64ToString(usecs);
      if (i < results->rawlen - 1)
        rawData += ","; // Добавляем запятую, кроме последнего элемента
    }
    return rawData;
  }

public:
  void begin() {
    recv.enableIRIn(); // Инициализация ИК-приемника
    send.begin();      // Инициализация ИК-передатчика
  }
  void update() {
    // Если включен режим ожидания ИК-сигнала
    if (isWaitingForIR && recv.decode(&results)) {
      lastIRData.hexcode = uint64ToString(results.value, HEX);
      lastIRData.protocol = typeToString(results.decode_type);
      lastIRData.raw = resultToRawArray(&results);
      DEBUG_PRINTF("Получен ИК-код: %s, Протокол:  %s", lastIRCode,
                   lastIRProtocol);
      DEBUG_PRINTF("RAW данные: %s", lastIRRaw);
      recv.pause();
      isWaitingForIR = false;
      webSocketMgr.notifyReceivedDataSetChanged(lastIRData);
      digitalWrite(LED_PIN, HIGH);
    }
  }

  void enableReceiver() {
    recv.enableIRIn();     // Инициализация ИК-приемника
    isWaitingForIR = true; // Устанавливаем флаг ожидания ИК-сигнала
  }

  void sendRaw(const uint16_t buf[], const uint16_t len, const uint16_t hz) {
    send.sendRaw(buf, len, hz); // TODO использваоть girs_sendraw
  }

  void resetLastIRData() {
    lastIRData.hexcode = waitText;
    lastIRData.protocol = waitText;
    lastIRData.raw = waitText;
  }

  IRData getLastIRData() { return lastIRData; }
};

#endif
