#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <ESP8266WiFi.h>
#include <IRsend.h>
#include <WiFiUdp.h>


extern IRsend irsend;
extern uint16_t irSendBuf[255]; // буфер для хранения RAW шаблона команды

class UDPServer {
  WiFiUDP udp;
  char udpBuffer[255];

public:
  void begin(uint16_t port) { udp.begin(port); }
  void update() {
    int packetSize = udp.parsePacket();
    if (!packetSize)
      return;

    if (udp.remoteIP() == WiFi.localIP()) { // Игнорируем свои же пакеты
      DEBUG_PRINTF("Получен свой UDP broadcast, игнорируем");
      return;
    }

    // Чтение данных из пакета
    int len =
        udp.read(udpBuffer,
                 sizeof(udpBuffer) - 1); // Оставляем место для нуль-терминатора
    if (len <= 0)
      return;

    udpBuffer[len] = '\0'; // Добавляем завершающий нулевой символ

    if (strcmp(udpBuffer, "IRHUB_ECHO") == 0) {
      String macAddress = WiFi.macAddress();
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write(macAddress.c_str(), macAddress.length());
      udp.endPacket();
      DEBUG_PRINTF("Отправлен MAC-адрес: %s на IP: %s", macAddress.c_str(),
                   udp.remoteIP().toString().c_str());
    } else if (memcmp(udpBuffer, "IRHUB_S01", 10) == 0) {
      digitalWrite(LED_PIN, LOW);
      DEBUG_PRINTF("Получена ИК команда от IP: %s",
                   udp.remoteIP().toString().c_str());
      uint16_t hz = 0;
      uint16_t pulses = 0;
      hz = (udpBuffer[10] << 8) | udpBuffer[11];
      pulses = (udpBuffer[12] << 8) | udpBuffer[13];
      for (uint16_t i = 0; i < pulses; i++) {
        irSendBuf[i] = (udpBuffer[14 + i * 2] << 8) | udpBuffer[14 + i * 2 + 1];
      }
      yield();
      // отправляем команду на ИК диод
      irsend.sendRaw(irSendBuf, pulses, hz);
      yield();
      DEBUG_PRINTF("hz: %i", hz);
      DEBUG_PRINTF("pulses: %i", pulses);
      DEBUG_PRINTF("irSendBuf[0]: %i", irSendBuf[0]);
      DEBUG_PRINTF("irSendBuf[pulses-1]: %i", irSendBuf[pulses - 1]);
      digitalWrite(LED_PIN, HIGH);
    }
  }

  /// Ретрансляция irSendBuf на остальне узлы сети по UDP
  /// @param[in] hz несущая частота
  /// @param[in] pulses размер выборки из буфера
  void sendUDPRawIR(uint16_t hz, uint16_t pulses) {
    char type[10] = "IRHUB_S01";

    // Заголовок 14 байт
    // 10 байт (тип) + 2 байта (hz) + 2 байта (длина) + данные
    uint8_t packet[14 + pulses * 2];

    memcpy(packet, type, 10);          // Тип пакета и версия формата
    packet[10] = (hz >> 8) & 0xFF;     // Старший байт hz
    packet[11] = hz & 0xFF;            // Младший байт hz
    packet[12] = (pulses >> 8) & 0xFF; // Старший байт длины
    packet[13] = pulses & 0xFF;        // Младший байт длины

    for (uint16_t i = 0; i < pulses; i++) {
      packet[14 + i * 2] = (irSendBuf[i] >> 8) & 0xFF;
      packet[14 + i * 2 + 1] = irSendBuf[i] & 0xFF;
    }

    // Устанавливаем широковещательный адрес
    IPAddress broadcastIP = WiFi.localIP();
    broadcastIP[3] = 255; // Последний октет адреса устанавливаем в 255 для
                          // широковещательной рассылки

    udp.beginPacket(broadcastIP, UDP_PORT);
    udp.write(packet, sizeof(packet));
    udp.endPacket();
    DEBUG_PRINTF("Отправлен UDP Broadcast RAW IR, размер пакета: %i",
                 sizeof(packet));
  }
};

#endif