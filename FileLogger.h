#pragma once

#include <fstream>
#include <atomic>
#include <thread>
#include <mutex>
#include <utility>
#include <filesystem>
#include <sstream>

#include "MemoryBoundedQueue.h"

class FileLogger {
 private:
  MemoryBoundedQueue<std::string> &queue_;
  std::ofstream file_stream_;
  std::atomic<bool> running_ = true;
  std::atomic<bool> wait_ = false;
  std::thread worker_;
  std::mutex mtx_;
  std::string filename_;
  const std::chrono::milliseconds flush_interval_ = std::chrono::milliseconds(100);
  const unsigned long max_file_size_;
  bool stopWorker = false;

  // Generate a filename based on the current date and time
  static std::string getFormattedFilename() {
    // Get current time as system_clock time_point
    auto now = std::chrono::system_clock::now();
    // Convert to time_t for compatibility with C time functions
    auto now_c = std::chrono::system_clock::to_time_t(now);
    // Convert to tm struct for use with put_time
    struct tm now_tm{};
    localtime_s(&now_tm, &now_c);

    // Use ostringstream to format filename
    std::ostringstream oss;
    oss << std::put_time(&now_tm, "syslog_%Y_%m_%d_%H_%M_%S.txt");

    return oss.str();
  }

  // Open a new log file with the current timestamp
  void openNewLogFile() {
    if (file_stream_.is_open()) {
      file_stream_.close();
    }
    filename_ = getFormattedFilename();
    file_stream_.open(filename_, std::ios::app);
  }

  // Check the size of the current log file and rotate if necessary
  void checkAndRotateFile() {
    if (std::filesystem::file_size(filename_) >= max_file_size_) {
      openNewLogFile();
    }
  }

  void backgroundScreenFlush() {
    while (!stopWorker) {
      std::this_thread::sleep_for(flush_interval_);
      std::lock_guard<std::mutex> lock(mtx_);
      file_stream_.flush();
    }
  }

 public:
  FileLogger(MemoryBoundedQueue<std::string> &q, unsigned long file_size)
      : filename_(std::move(getFormattedFilename())),
        max_file_size_(file_size),
        queue_(q) {
    worker_ = std::thread(&::FileLogger::backgroundScreenFlush, this);
  }

  ~FileLogger() {
    stopWorker = true;
    worker_.join();
    if (file_stream_.is_open()) {
      file_stream_.close();
    }
  }

  void run() {
    file_stream_.open(filename_, std::ios::app);
    unsigned int count = 0;
    while (running_) {
      std::string log = queue_.pop();
      std::lock_guard<std::mutex> lock(mtx_);
      file_stream_ << log << std::endl;
      checkAndRotateFile();
    }
    file_stream_.flush(); // in case wait_ is used and takes time
    if (wait_) {
      while (!queue_.empty()) {
        std::string log = queue_.pop();
        std::lock_guard<std::mutex> lock(mtx_);
        file_stream_ << log << std::endl;
        checkAndRotateFile();
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