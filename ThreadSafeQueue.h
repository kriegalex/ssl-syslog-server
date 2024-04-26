#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class ThreadSafeQueue {
 protected:
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  std::queue<T> queue_;

 public:
  virtual void push(T value) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(std::move(value));
    condition_variable_.notify_one();
  }

  virtual T pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_variable_.wait(lock, [this] { return !queue_.empty(); });
    T tmp = std::move(queue_.front());
    queue_.pop();
    return tmp;
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  bool size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }
};