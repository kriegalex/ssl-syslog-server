#pragma once

#include <fstream>
#include <atomic>
#include <utility>

#include "MemoryBoundedQueue.h"

class FileLogger {
 private:
  MemoryBoundedQueue<std::string> &queue_;
  std::ofstream file_stream_;
  std::atomic<bool> running_;
  std::atomic<bool> wait_;
  std::string filename_;
  unsigned long max_file_size_;

 public:
  FileLogger(MemoryBoundedQueue<std::string> &q, std::string f, unsigned long file_size)
      : filename_(std::move(f)), max_file_size_(file_size), queue_(q), running_(true), wait_(false) {
  }

  ~FileLogger() {
    if (file_stream_.is_open()) {
      file_stream_.close();
    }
  }

  void run() {
    file_stream_.open(filename_, std::ios::app);
    while (running_) {
      std::string log = queue_.pop();
      file_stream_ << log << std::endl;
    }
    file_stream_.flush();
    if(wait_) {
      while(!queue_.empty()) {
        std::string log = queue_.pop();
        file_stream_ << log << std::endl;
      }
      file_stream_.flush();
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