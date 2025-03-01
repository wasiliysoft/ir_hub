

#define INIT_ADDR 1023  // номер ячейки для хранения клоюча первого запуска
#define INIT_KEY 53     // ключ первого запуска. 0-254, на выбор



void loadSettings() {
  if (EEPROM.read(INIT_ADDR) != INIT_KEY) {  // первый запуск
    delay(5000);
    Serial.println("Первый запуск");
    Serial.println("Первый запуск");
    Serial.println("Первый запуск");
    EEPROM.write(INIT_ADDR, INIT_KEY);  // записали ключ
    setDefaultSettings();
    saveSettings();
  } else {
    EEPROM.get(0, settings);
  }
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void setDefaultSettings() {
  strcpy(settings.ssid, SSID_DEFAULT);
  strcpy(settings.password, "");
  settings.isAPMode = true;
}