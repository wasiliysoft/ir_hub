#include "main.h"

void UDPTick() {
  int packetSize = udpServer.parsePacket();
  if (packetSize) {
    Serial.printf(
        "Received packet of size %d from %s:%d\n (to %s:%d, free "
        "heap = %d B)\n",
        packetSize, udpServer.remoteIP().toString().c_str(), udpServer.remotePort(),
        udpServer.destinationIP().toString().c_str(), udpServer.localPort(), ESP.getFreeHeap());
    yield();
    // read the packet into packetBufffer
    int n = udpServer.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    packetBuffer[n] = 0;
    yield();
    if (String(packetBuffer) == "IRHUB_ECHO") {
      sendUDP(String(WiFi.macAddress()).c_str(), udpServer.remoteIP(), udpServer.remotePort());
    } else {
      Serial.println("Contents:");
      Serial.println(packetBuffer);
    }
    yield();
  }
}

void sendUDP(const char *data, IPAddress address, uint16_t port) {
  udpServer.beginPacket(address, port);
  udpServer.write(data);
  udpServer.endPacket();
  yield();
}

void restartUDP() {
  udpServer.stop();
  udpServer.begin(UDP_PORT);
  Serial.printf("UDP server on port %d\n", UDP_PORT);
  yield();
}
