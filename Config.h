#ifndef SYSLOGCONFIG_H
#define SYSLOGCONFIG_H

#include <unordered_map>
#include <string>
#include <array>

class Config {
 public:
  explicit Config(const std::string &configPath);
  int getServerPort() const;
  int getErrorSeverityColorCode() const;
  int getInfoSeverityColorCode() const;
  int getDebugSeverityColorCode() const;
  unsigned long getFileMaxSizeKb() const;
  bool isOutputToScreen() const;
  unsigned long getMaxMemorySizeKb() const;

 private:
  int server_port_ = 60119;
  bool output_to_screen_ = false;
  unsigned long syslog_file_max_size_kb_ = 1000; // 1MB
  unsigned long syslog_max_memory_size_kb_ = 1000000; // 1GB
  std::unordered_map<std::string, int> priorityColors;
  void loadConfig(const std::string &path);
  const std::array<std::string, 3> levels = {"error", "info", "debug"};
  const std::unordered_map<std::string, int> defaultColors = {{"error", 12}, {"info", 1}, {"debug", 8}};
  const std::unordered_map<std::string, int> winTerminalColors = {
      {"BLACK", 0},
      {"BLUE", 1},
      {"GREEN", 2},
      {"CYAN", 3},
      {"RED", 4},
      {"MAGENTA", 5},
      {"YELLOW", 6},
      {"WHITE", 7},
      {"BRIGHT_BLACK", 8},
      {"BRIGHT_BLUE", 9},
      {"BRIGHT_GREEN", 10},
      {"BRIGHT_CYAN", 11},
      {"BRIGHT_RED", 12},
      {"BRIGHT_MAGENTA", 13},
      {"BRIGHT_YELLOW", 14},
      {"BRIGHT_WHITE", 15}};
};

#endif
