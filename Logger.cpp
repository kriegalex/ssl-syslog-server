#include "Logger.h"

#include <filesystem>
#include <sstream>

#include "ScreenLogger.h"
#include "FileLogger.h"

Logger::Logger(const Config &cfg) : screen_queue_(MemoryBoundedQueue<std::string>(cfg.getMaxMemorySizeKb() * 1024)),
                                    file_queue_(MemoryBoundedQueue<std::string>(cfg.getMaxMemorySizeKb() * 1024)),
                                    screen_logger_(screen_queue_),
                                    file_logger_(file_queue_, getFormattedFilename(), cfg.getFileMaxSizeKb() * 1024),
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

void Logger::processMessage(const char *input) {
  message_ = std::string(input);
  int length = extractMessageLength();
  if (length != (message_.length() - message_.find(' '))) {
    std::cerr << "Error: Message length does not match the expected length!" << std::endl;
    return;
  }

  int priorityDigit = extractPriorityDigit();
  int colorCode = getColorCode(priorityDigit);
  if (colorCode == 0) {
    std::cerr << "Unsupported priority digit for color mapping." << std::endl;
    return;
  }

  std::string formattedMessage = formatMessage();
  file_queue_.push(formattedMessage);
  std::string ansiColor = getAnsiColorCode(colorCode); // Assume this function is defined elsewhere
  // prefix with color and append with reset color
  const std::string screenStr = ansiColor + formattedMessage + std::string("\x1b[0m");
  if (is_output_to_screen_) {
    screen_queue_.push(screenStr);
  }
}

int Logger::extractMessageLength() const {
  size_t firstSpaceIndex = message_.find(' ');
  return std::stoi(message_.substr(0, firstSpaceIndex));
}

int Logger::extractPriorityDigit() const {
  size_t start = message_.find('<') + 1;
  size_t end = message_.find('>');
  return std::stoi(message_.substr(start, end - start)) % 8;
}

int Logger::getColorCode(int priorityDigit) const {
  auto it = priority_color_map_.find(priorityDigit);
  if (it != priority_color_map_.end()) {
    return it->second;
  }
  return 0; // Return 0 if not found to signify unsupported priority
}

std::string Logger::formatMessage() const {
  size_t firstSpaceIndex = message_.find(' ');
  return message_.substr(firstSpaceIndex + 1);
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

// Generate a filename based on the current date and time
std::string Logger::getFormattedFilename() {
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
