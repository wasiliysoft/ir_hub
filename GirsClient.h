#ifndef GIRSCLIENT_H
#define GIRSCLIENT_H

#include "config.h"
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// Объявление внешних переменных (определены в .ino файле)
extern IRsend irsend;

// Прототипы функций
void girs_begin();
void girs_tic();
void girs_processCommand(const String& line, Stream& stream);
void girs_receive(Stream& stream);
void girs_sendRaw(const uint16_t intro[], unsigned lengthIntro, const uint16_t repeat[], unsigned lengthRepeat, const uint16_t ending[], unsigned lengthEnding, uint16_t frequency, unsigned times);
String girs_getNextToken(String& str);

#endif  // GIRSCLIENT_H