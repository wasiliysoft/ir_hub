#ifndef RING_BUFFER_H
#define RING_BUFFER_H
#include <cstdint>

// Простой кольцевой буфер для embedded
class RingBuffer {
private:
  volatile uint16_t head = 0;  // позиция записи
  volatile uint16_t tail = 0;  // позиция чтения
  const uint16_t capacity;     // вместимость буфера
  uint8_t* buffer;             // динамический буфер
public:
  // Конструктор: выделяет память под буфер заданного размера
  explicit RingBuffer(uint16_t size) : capacity(size), buffer(new uint8_t[size]) {}

  // Деструктор: освобождает память
  ~RingBuffer() { delete[] buffer; }

  // Запрещаем копирование (типично для embedded, чтобы избежать ошибок)
  RingBuffer(const RingBuffer&) = delete;
  RingBuffer& operator=(const RingBuffer&) = delete;

  void push(uint8_t byte) {
    uint16_t nextHead = (head + 1) % capacity;
    if (nextHead != tail) {  // буфер не полон
      buffer[head] = byte;
      head = nextHead;
    }
    // Иначе — игнорируем байт (переполнение)
  }

  bool empty() const { return head == tail; }

  uint16_t available() const {
    if (head >= tail) {
      return head - tail;
    }
    return capacity - tail + head;
  }

  uint8_t front() const {
    if (!empty()) {
      return buffer[tail];
    }
    return 0;
  }

  uint8_t pop() {
    if (!empty()) {
      uint8_t byte = buffer[tail];
      tail = (tail + 1) % capacity;
      return byte;
    }
    return 0;
  }

  void clear() { tail = head; }

  uint16_t getCapacity() const { return capacity; }
};

#endif