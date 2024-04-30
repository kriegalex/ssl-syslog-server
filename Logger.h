#pragma once

#include <string>
#include <unordered_map>

#include "Config.h"
#include "ThreadSafeQueue.h"
#include "ScreenLogger.h"
#include "FileLogger.h"

class Logger {
 public:
  explicit Logger(const Config &cfg);
  virtual ~Logger();
  void processMessage(const char *input);
  void stopWaitLoggers();

 private:
//  SyslogBatcher batcher;
  MemoryBoundedQueue<std::string> screen_queue_;
  MemoryBoundedQueue<std::string> file_queue_;
  ScreenLogger screen_logger_;
  FileLogger file_logger_;
  std::thread screen_thread_;
  std::thread file_thread_;
  std::string message_;
  /*
   * the key is an int and represents the severity level encoded in the syslog message
   * <166> -> level 6 (facility*8+severity)
   */
  std::unordered_map<int, int> priority_color_map_;
  bool is_output_to_screen_ = false;

  int extractMessageLength() const;
  int extractPriorityDigit() const;
  std::string formatMessage() const;
  static std::string getAnsiColorCode(int colorCode);
  int getColorCode(int priorityDigit) const;
  void stopLoggers();
};
