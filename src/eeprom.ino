#include "main.h"

#define EE_KEY 56  // ключ сброса eeprom

void EE_startup() {
  EEPROM.begin(1000);  // старт епром
  delay(100);
  if (EEPROM.read(0) != EE_KEY) {
    Serial.println("First start, skip reading eeprom");
    return;
  }
  EEPROM.get(1, cfg);
  yield();
  Serial.println();
  Serial.print("EEPROM read size: ");
  Serial.println(sizeof(cfg) + 1);
  yield();
  Serial.print("SSID ");
  Serial.print(cfg.SSID);
  Serial.print(" pass ");
  Serial.print(cfg.pass);
  yield();
  Serial.print(" WiFi mode ");
  Serial.println(cfg.mode);
  Serial.println();
}

void EE_save() {
  Serial.println("save cfg");
  if (EEPROM.read(0) != EE_KEY) {
    EEPROM.write(0, EE_KEY);
  }
  EEPROM.put(1, cfg);
  EEPROM.commit();
  yield();
}
