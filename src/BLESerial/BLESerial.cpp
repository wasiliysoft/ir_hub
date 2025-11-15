#ifdef ESP32
// BLESerial.cpp
#include "BLESerial.h"

BLESerial::BLESerial() {
  deviceConnected = false;
  pServer = nullptr;
  pTxCharacteristic = nullptr;
  pRxCharacteristic = nullptr;

  // Инициализируем кольцевой буфер
  rxBufferSize = 4096;  // Можно изменить по необходимости
  rxBuffer = new uint8_t[rxBufferSize];
  rxBufferHead = 0;
  rxBufferTail = 0;
}

BLESerial::~BLESerial() {
  end();
  delete[] rxBuffer;
}

void BLESerial::begin(const char* deviceName) {
  Serial.println("Initializing BLE...");

  BLEDevice::init(deviceName);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks(this));

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new RxCallbacks(this));

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("BLE Serial started successfully");
  Serial.println("Waiting for client connection...");
}

void BLESerial::end() {
  if (pServer) {
    pServer->getAdvertising()->stop();
  }
  deviceConnected = false;
}

void BLESerial::addToRxBuffer(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    // Пропускаем символ \n (LF)
    if (data[i] == '\n') {
      continue;
    }
    size_t nextHead = (rxBufferHead + 1) % rxBufferSize;

    // Если буфер полон, перезаписываем старые данные
    if (nextHead == rxBufferTail) {
      rxBufferTail = (rxBufferTail + 1) % rxBufferSize;
    }

    rxBuffer[rxBufferHead] = data[i];
    rxBufferHead = nextHead;
  }
}

int BLESerial::available() {
  if (rxBufferHead >= rxBufferTail) {
    return rxBufferHead - rxBufferTail;
  } else {
    return rxBufferSize - rxBufferTail + rxBufferHead;
  }
}

int BLESerial::read() {
  if (rxBufferTail == rxBufferHead) {
    return -1;  // Буфер пуст
  }

  uint8_t data = rxBuffer[rxBufferTail];
  rxBufferTail = (rxBufferTail + 1) % rxBufferSize;
  return data;
}

int BLESerial::peek() {
  if (rxBufferTail == rxBufferHead) {
    return -1;  // Буфер пуст
  }

  return rxBuffer[rxBufferTail];
}

void BLESerial::flush() {
  // Для BLE flush обычно не требуется
}

size_t BLESerial::write(uint8_t data) {
  if (!deviceConnected) return 0;
  uint8_t buffer[1] = {data};
  pTxCharacteristic->setValue(buffer, 1);
  pTxCharacteristic->notify();
  return 1;
}

size_t BLESerial::write(const uint8_t* buffer, size_t size) {
  if (!deviceConnected || !buffer || size == 0) return 0;

  pTxCharacteristic->setValue(const_cast<uint8_t*>(buffer), size);
  pTxCharacteristic->notify();
  return size;
}
#endif