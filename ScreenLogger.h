#pragma once

#include <iostream>
#include <string>
#include <atomic>

#include "MemoryBoundedQueue.h"

class ScreenLogger {
 private:
  MemoryBoundedQueue<std::string> &queue_;
  std::atomic<bool> running_;
  std::atomic<bool> wait_;

 public:
  explicit ScreenLogger(MemoryBoundedQueue<std::string> &q) : queue_(q), running_(true), wait_(false) {}

  void run() {
    while (running_) {
      std::string log = queue_.pop();
      std::cout << log << std::endl;
    }
    if(wait_) {
      while(!queue_.empty()) {
        std::string log = queue_.pop();
        std::cout << log << std::endl;
      }
    }
  }

  void stop() {
    wait_ = false;
    running_ = false;
  }

  void stopWaitFinished() {
    wait_ = true;
    running_ = false;
  }
};
