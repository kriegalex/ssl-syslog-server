#pragma once

#include "ThreadSafeQueue.h"

template<typename T>
class MemoryBoundedQueue : public ThreadSafeQueue<T> {
 private:
  size_t max_memory_bytes_;
  size_t current_memory_bytes_ = 0;

  size_t estimateMemoryUsage(const T &item);

 public:

  explicit MemoryBoundedQueue(size_t max_memory_bytes) : max_memory_bytes_(max_memory_bytes) {}
  void push(T value) override {
    std::unique_lock<std::mutex> lock(this->mutex_);
    size_t item_size = estimateMemoryUsage(value);
    this->condition_variable_.wait(lock,
                                   [this, item_size] {
                                     return (current_memory_bytes_ + item_size) <= max_memory_bytes_;
                                   });

    this->queue_.push(value);
    current_memory_bytes_ += item_size;
    lock.unlock();
    this->condition_variable_.notify_one();
  }
  T pop() override {
    std::unique_lock<std::mutex> lock(this->mutex_);
    this->condition_variable_.wait(lock, [this] { return !this->queue_.empty(); });

    T tmp = std::move(this->queue_.front());
    this->queue_.pop();
    current_memory_bytes_ -= estimateMemoryUsage(tmp);
    lock.unlock();
    this->condition_variable_.notify_one();
    return tmp;
  }
  size_t getCurrentMemoryUsage() const {
    std::lock_guard<std::mutex> lock(this->mutex_);
    return current_memory_bytes_;
  }
};
