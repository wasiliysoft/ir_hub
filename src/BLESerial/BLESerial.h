// BLESerial.h
#ifdef ESP32
#ifndef BLESERIAL_H
#define BLESERIAL_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class BLESerial : public Stream {
private:
  BLEServer* pServer;
  BLECharacteristic* pTxCharacteristic;
  BLECharacteristic* pRxCharacteristic;

  bool deviceConnected;

  // Заменяем String на более эффективный буфер
  uint8_t* rxBuffer;
  size_t rxBufferSize;
  size_t rxBufferHead;
  size_t rxBufferTail;

  class ServerCallbacks : public BLEServerCallbacks {
  private:
    BLESerial* bleSerial;

  public:
    ServerCallbacks(BLESerial* serial) : bleSerial(serial) {}

    void onConnect(BLEServer* pServer) {
      bleSerial->deviceConnected = true;
      Serial.println("BLE device connected");
    }

    void onDisconnect(BLEServer* pServer) {
      bleSerial->deviceConnected = false;
      Serial.println("BLE device disconnected");
      
    }
  };

  class RxCallbacks : public BLECharacteristicCallbacks {
  private:
    BLESerial* bleSerial;

  public:
    RxCallbacks(BLESerial* serial) : bleSerial(serial) {}

    void onWrite(BLECharacteristic* pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        bleSerial->addToRxBuffer((uint8_t*)value.data(), value.length());
      }
    }
  };

  // Вспомогательный метод для добавления данных в буфер
  void addToRxBuffer(uint8_t* data, size_t length);

public:
  BLESerial();
  ~BLESerial();

  void begin(const char* deviceName = "ESP32-C3-BLE-UART");
  void end();

  // Методы интерфейса Stream
  int available() override;
  int read() override;
  int peek() override;
  void flush() override;

  // Методы интерфейса Print
  size_t write(uint8_t data) override;
  size_t write(const uint8_t* buffer, size_t size) override;

  using Print::write;

  bool isConnected() { return deviceConnected; }
  void clearBuffer() { 
    rxBufferHead = 0;
    rxBufferTail = 0;
  }
};

#endif  // BLESERIAL_H
#endif