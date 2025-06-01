void initialWiFi() {
  if (settings.isAPMode) {
    startAPMode();
  } else {
    startClientMode();
  }
}
void startClientMode() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, LOW);
    delay(400);
    digitalWrite(LED_PIN, HIGH);
    delay(400);
    Serial.print(".");
    if (millis() > 10000) {
      Serial.println("Не удается подключиться к сети, включаем точку доступа");
      setDefaultSettings();
      startAPMode();
      return;
    }
  }
  Serial.println("");
  Serial.println("WiFi подключен");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(settings.ssid, settings.password);
  Serial.println("Точка доступа запущена");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  // Запуск DNS-сервера для перенаправления всех запросов на IP ESP8266
  dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.println("DNS сервер запущен");
}
