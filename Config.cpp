#include "Config.h"
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

Config::Config(const std::string &configPath) {
  loadConfig(configPath);
}

void Config::loadConfig(const std::string &path) {
  std::ifstream configFile(path);
  json configJson;
  configFile >> configJson;

  server_port_ = configJson["server_port"];
  output_to_screen_ = configJson["screen_output"];
  syslog_file_max_size_kb_ = configJson["file_max_size_kb"];
  syslog_max_memory_size_kb_ = configJson["max_memory_size_kb"];
  auto colors = configJson["priority_colors"];
  for (const auto &elt : levels) {
    // set default then check config.json
    priorityColors[elt] = defaultColors.at(elt);
    // info, debug, ... exist as keys in the config.json
    if (colors.find(elt) != colors.end()) {
      // the given color strings in config.json exist in our mapping table
      if (winTerminalColors.find(colors[elt]) != winTerminalColors.end()) {
        priorityColors[elt] = winTerminalColors.at(colors[elt]);
      }
    }
  }
}

unsigned long Config::getMaxMemorySizeKb() const {
  return syslog_max_memory_size_kb_;
}

int Config::getServerPort() const {
  return server_port_;
}

int Config::getErrorSeverityColorCode() const {
  return priorityColors.at("error");
}

int Config::getInfoSeverityColorCode() const {
  return priorityColors.at("info");
}

int Config::getDebugSeverityColorCode() const {
  return priorityColors.at("debug");
}

unsigned long Config::getFileMaxSizeKb() const {
  return syslog_file_max_size_kb_;
}

bool Config::isOutputToScreen() const {
  return output_to_screen_;
}
