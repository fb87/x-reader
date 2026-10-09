#pragma once

#include <cstddef>

/**
 * @brief Fixed-capacity FIFO queue with no dynamic allocation.
 */
namespace fixed_queue {

template <typename T, std::size_t Capacity>
struct queue {
  static_assert(Capacity > 0);
  T values[Capacity]{};
  std::size_t head = 0;
  std::size_t tail = 0;
};

template <typename T, std::size_t Capacity>
inline bool empty(const queue<T, Capacity>& self) {
  return self.head == self.tail;
}

template <typename T, std::size_t Capacity>
inline std::size_t size(const queue<T, Capacity>& self) {
  return self.tail - self.head;
}

template <typename T, std::size_t Capacity>
inline bool push(queue<T, Capacity>& self, const T& value) {
  if (size(self) >= Capacity) {
    return false;
  }
  self.values[self.tail % Capacity] = value;
  ++self.tail;
  return true;
}

template <typename T, std::size_t Capacity>
inline bool pop(queue<T, Capacity>& self, T& value) {
  if (empty(self)) {
    return false;
  }
  value = self.values[self.head % Capacity];
  ++self.head;
  return true;
}

template <typename T, std::size_t Capacity>
inline void clear(queue<T, Capacity>& self) {
  self.head = 0;
  self.tail = 0;
}

}  // namespace fixed_queue
