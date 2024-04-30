#include "MemoryBoundedQueue.h"

#include <string>

template<typename T>
size_t MemoryBoundedQueue<T>::estimateMemoryUsage(const T& item) {
  return sizeof(item);
}

template<>
size_t MemoryBoundedQueue<std::string>::estimateMemoryUsage(const std::string &item) {
  return sizeof(std::string) + item.capacity() * sizeof(char);
}