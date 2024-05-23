#include "Logger.h"

#include <sstream>

#include "ScreenLogger.h"
#include "FileLogger.h"

Logger::Logger(const Config &cfg) : screen_queue_(MemoryBoundedQueue<std::string>(cfg.getMaxMemorySizeKb() * 1024)),
                                    file_queue_(MemoryBoundedQueue<std::string>(cfg.getMaxMemorySizeKb() * 1024)),
                                    screen_logger_(screen_queue_),
                                    file_logger_(file_queue_, cfg.getFileMaxSizeKb() * 1024),
                                    is_output_to_screen_(cfg.isOutputToScreen()) {
  priority_color_map_ = {
      {3, cfg.getErrorSeverityColorCode()},
      {6, cfg.getInfoSeverityColorCode()},
      {7, cfg.getDebugSeverityColorCode()}};
  file_thread_ = std::thread(&FileLogger::run, &file_logger_);
  if (is_output_to_screen_)
    screen_thread_ = std::thread(&ScreenLogger::run, &screen_logger_);
}

Logger::~Logger() {
  stopLoggers();
  if (file_thread_.joinable()) {
    file_thread_.join();
  }
  if (is_output_to_screen_ && screen_thread_.joinable()) {
    screen_thread_.join();
  }
}

size_t Logger::processMessage(const char *input) {
  message_ = std::string(input);

  file_queue_.push(message_);
  // prefix with color and append with reset color
  const std::string screenStr = message_;
  if (is_output_to_screen_) {
    screen_queue_.push(screenStr);
  }
  return message_.length(); // return the processed size
}

void Logger::startColorLine(int priority_digit) {
  if (is_output_to_screen_) {
    screen_queue_.push(getAnsiColorCode(getColorCode(priority_digit)));
  }
}

void Logger::endLine() {
  file_queue_.push("\n");
  if (is_output_to_screen_) {
    screen_queue_.push("\x1b[0m\n");
  }
}

int Logger::getColorCode(int priority_digit) const {
  auto it = priority_color_map_.find(priority_digit);
  if (it != priority_color_map_.end()) {
    return it->second;
  }
  return 1000; // Return 1000 if not found to signify unsupported priority
}

std::string Logger::getAnsiColorCode(int colorCode) {
  switch (colorCode) {
    case 0: return "\x1b[30m"; // Black
    case 1: return "\x1b[34m"; // Blue
    case 2: return "\x1b[32m"; // Green
    case 3: return "\x1b[36m"; // Cyan
    case 4: return "\x1b[31m"; // Red
    case 5: return "\x1b[35m"; // Magenta
    case 6: return "\x1b[33m"; // Yellow
    case 7: return "\x1b[37m"; // White
    case 8: return "\x1b[90m"; // Bright Black (Gray)
    case 9: return "\x1b[94m"; // Bright Blue
    case 10: return "\x1b[92m"; // Bright Green
    case 11: return "\x1b[96m"; // Bright Cyan
    case 12: return "\x1b[91m"; // Bright Red
    case 13: return "\x1b[95m"; // Bright Magenta
    case 14: return "\x1b[93m"; // Bright Yellow
    case 15: return "\x1b[97m"; // Bright White
    default: return "\x1b[0m"; // Reset
  }
}

void Logger::stopLoggers() {
  screen_logger_.stop();
  file_logger_.stop();
  // Push empty messages to unblock queues if they are waiting
  screen_queue_.push("");
  file_queue_.push("");
}

void Logger::stopWaitLoggers() {
  screen_logger_.stopWaitFinished();
  file_logger_.stopWaitFinished();
  // Push empty messages to unblock queues if they are waiting
  screen_queue_.push("");
  file_queue_.push("");
  if (file_thread_.joinable()) {
    file_thread_.join();
  }
  if (is_output_to_screen_ && screen_thread_.joinable()) {
    screen_thread_.join();
  }
}
