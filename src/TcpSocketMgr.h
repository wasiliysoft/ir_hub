#ifndef TCP_SOCKET_MGR_H
#define TCP_SOCKET_MGR_H

#include <ESP8266WiFi.h>
#include "lib/RingBuffer.h"

#ifndef IRHUB_CONFIG_H
#include "config.h"
#endif


class TcpSocketMgr : public Stream {
private:
  WiFiServer server;
  WiFiClient clients[4];           // Фиксированный массив вместо vector
  bool clientActive[4] = {false};  // Флаг активности клиента
  const uint16_t port;
  RingBuffer rxBuffer;  // Кольцевой буфер
  static const uint8_t MAX_CLIENTS = 4;

public:
  TcpSocketMgr(uint16_t port = 33333) : server(port), port(port), rxBuffer(512) {}

  // === Методы Stream ===
  int available() override {
    checkForNewData();
    return rxBuffer.available();
  }

  int peek() override {
    checkForNewData();
    if (rxBuffer.available() > 0) {
      return rxBuffer.front();
    }
    return -1;
  }

  int read() override {
    checkForNewData();
    if (rxBuffer.available() > 0) {
      return rxBuffer.pop();
    }
    return -1;
  }

  size_t write(uint8_t byte) override {
    size_t written = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        written += clients[i].write(byte);
      }
    }
    return written > 0 ? 1 : 0;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    size_t total = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        total += clients[i].write(buffer, size);
      }
    }
    return total;
  }

  void flush() override {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        clients[i].flush();
      }
    }
  }

  // === Управление сервером ===
  void begin() {
    server.begin();
    server.setNoDelay(true);
    Serial.printf("TCP Socket server started on port: %d\n", port);
  }

  void update() {
    handleNewConnections();
    checkForDisconnectedClients();
    checkForNewData();  // Всегда обновляем данные
  }

  // === Обработка клиентов ===
  void handleNewConnections() {
    if (server.hasClient()) {
      WiFiClient newClient = server.accept();
      if (!newClient) return;

      // Поиск свободного слота
      for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clientActive[i]) {
          clients[i] = newClient;
          clientActive[i] = true;
          clients[i].setNoDelay(true);

#ifdef DEBUG
          IPAddress ip = clients[i].remoteIP();
          DEBUG_PRINTF("New client connected: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
#endif
          clients[i].print("Welcome to IR Hub TCP Server!\n");
          return;
        }
      }

      // Все слоты заняты
      newClient.print("Server connection limit. Try later.\n");
      newClient.stop();
      Serial.println("TCP Server client limit reached");
    }
  }

  void checkForDisconnectedClients() {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && !clients[i].connected()) {
        clients[i].stop();
        clientActive[i] = false;
      }
    }
  }

  void checkForNewData() {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        while (clients[i].available()) {
          rxBuffer.push(clients[i].read());  // Кольцевой буфер сам управляет переполнением
        }
      }
    }
  }

  // === Отправка данных ===
  void broadcastTXT(const String& text) { broadcastTXT(text.c_str()); }

  void broadcastTXT(const char* text) {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        clients[i].print(text);
      }
    }
  }

  void broadcastBIN(const uint8_t* data, size_t length) {
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        clients[i].write(data, length);
      }
    }
  }

  uint8_t connectedClients() {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
      if (clientActive[i] && clients[i].connected()) {
        count++;
      }
    }
    return count;
  }

  uint16_t getPort() const { return port; }

  // Поддержка Print
  using Print::write;
  using Print::print;
  using Print::println;
};

#endif
