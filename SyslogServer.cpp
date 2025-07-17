#include "SyslogServer.h"

#include <csignal>
#include <utility>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <openssl/err.h>

using namespace std::string_literals;

// Initialize static instance pointer
SyslogServer *SyslogServer::instance_ = nullptr;

SyslogServer::SyslogServer(const std::string &configPath)
    : config_(configPath), logger_ptr_(std::make_shared<Logger>(config_)) {
  instance_ = this;
  ssl_ctx_ = SSLUtil::createServerContext();
  SSLUtil::initWinSocket();
  server_socket_ = SSLUtil::createSocket(config_.getServerPort());
  setupSignals();
  enableVirtualTerminalProcessing();
}

SyslogServer::~SyslogServer() {
  cleanup();
  SSLUtil::cleanWinSocket();
}

void SyslogServer::shutdownServer(int sig) {
  if (sig != SIGINT) // unexpected
    return;
  std::cout << "Shutdown signal received" << std::endl;
  if (instance_->running_) {
    instance_->running_ = false;
    instance_->cleanup();
  }
}

void SyslogServer::setupSignals() {
  signal(SIGINT, shutdownServer); // Simple signal handler
}

void SyslogServer::enableVirtualTerminalProcessing() {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) {
    return;
  }

  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) {
    return;
  }

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) {
    return;
  }
}

void SyslogServer::run() {
  std::cout << std::endl
            << __DATE__ << " " << __TIME__
            << " Syslog server running on port " << config_.getServerPort()
            << std::endl
            << std::endl;
  running_ = true;
  acceptConnections();
}

void SyslogServer::acceptConnections() {
  while (instance_->running_) {
    int client_socket = SSLUtil::acceptClient(server_socket_);
    if (client_socket != INVALID_SOCKET) {
      SSLUtil::setupClient(client_socket);

      std::string client_ip = SSLUtil::getClientIP(client_socket);
      std::cout << "Client connected: " << client_ip << std::endl;
      SSL *ssl = SSLUtil::createSSL(ssl_ctx_, client_socket);
      // use shared pointer
      auto thread = std::make_shared<SyslogServerThread>(ssl, client_socket, client_ip, logger_ptr_);

      { // save as weak_ptr to signal later without increasing ownership count
        std::lock_guard<std::mutex> lock(shutdown_mutex_);
        threads_.emplace_back(thread);
      }

      // use lambda to create a new thread and add it to the vector
      std::thread([thread]() {
        thread->run();
      }).detach();
    }
  }
}

void SyslogServer::cleanup() {
  std::lock_guard<std::mutex> lock(shutdown_mutex_);
  for (const auto &weak_thread : threads_) {
    if (auto thread = weak_thread.lock()) {
      thread->clientCleanup();
    }
  }
  serverCleanup();
}

void SyslogServer::serverCleanup() {
  if (server_socket_ != -1) {
    std::cout << "Cleaning up server socket..." << std::endl;
    closesocket(server_socket_);
    server_socket_ = -1;
  }
}

SyslogServerThread::SyslogServerThread(SSL *ssl,
                                       int client_socket,
                                       std::string client_ip,
                                       std::shared_ptr<Logger> logger_ptr)
    : ssl_(ssl), client_socket_(client_socket), client_ip_(std::move(client_ip)), logger_ptr_(std::move(logger_ptr)) {}

int SyslogServerThread::extractPriorityDigit(const std::string &message) {
  size_t start = message.find('<');
  size_t end = message.find('>');
  
  if (start == std::string::npos || end == std::string::npos || start >= end) {
    std::cerr << "Invalid priority format in message: " << message << std::endl;
    return 0; // Default to emergency level
  }
  
  try {
    std::string priority_str = message.substr(start + 1, end - start - 1);
    int priority_val = std::stoi(priority_str);
    return priority_val % 8; // Extract severity level
  } catch (const std::exception& e) {
    std::cerr << "Failed to parse priority: " << e.what() << std::endl;
    return 0; // Default to emergency level
  }
}

// Function to remove BOM
std::string SyslogServerThread::removeBOM(const std::string &data) {
  const std::string BOM_UTF8 = "\xEF\xBB\xBF"s;
  const std::string BOM_UTF16_BE = "\xFE\xFF"s;
  const std::string BOM_UTF16_LE = "\xFF\xFE"s;
  const std::string BOM_UTF32_BE = "\x00\x00\xFE\xFF"s;
  const std::string BOM_UTF32_LE = "\xFF\xFE\x00\x00"s;

  if (data.compare(0, BOM_UTF8.size(), BOM_UTF8) == 0) {
    return data.substr(BOM_UTF8.size());
  } else if (data.compare(0, BOM_UTF16_BE.size(), BOM_UTF16_BE) == 0 ||
      data.compare(0, BOM_UTF16_LE.size(), BOM_UTF16_LE) == 0) {
    return data.substr(2);
  } else if (data.compare(0, BOM_UTF32_BE.size(), BOM_UTF32_BE) == 0 ||
      data.compare(0, BOM_UTF32_LE.size(), BOM_UTF32_LE) == 0) {
    return data.substr(4);
  }
  return data;
}

// Function to ensure the message is in UTF-8 encoding
std::string SyslogServerThread::normalizeToUTF8(const std::string &message) {
  // UTF-16 and UTF-32 not supported for now
  if (message.size() >= 2 && message[0] != '\xEF') {
    return "Unsupported UTF-16/32 encoding";
  }
  // Remove BOM if present
  std::string cleaned_message = removeBOM(message);

  return std::move(cleaned_message); // Assuming message is already in UTF-8 if no BOM or UTF-16 detected
}

std::string SyslogServerThread::parseSyslogMsg(const char *msg) {
  if (!msg) {
    throw std::invalid_argument("Null message pointer");
  }
  
  const std::string input = std::string(msg);
  if (input.empty()) {
    throw std::invalid_argument("Empty message");
  }
  
  std::istringstream stream(input);
  std::string message_content;

  // Example syslog message format: "<34>1 2020-12-31T23:59:59Z mymachine app 1234 - [exampleSDID@32473 iut=\"3\" eventSource=\"Application\" eventID=\"1011\"] BOM and message content"

  // Clear previous values
  priority_.clear();
  timestamp_.clear();
  hostname_.clear();
  app_name_.clear();
  process_id_.clear();
  message_id_.clear();
  hyphen_.clear();

  // Parse first 6 fields (up to message_id)
  if (!(stream >> priority_ >> timestamp_ >> hostname_ >> app_name_ >> process_id_ >> message_id_)) {
    throw std::runtime_error("Failed to parse required syslog fields");
  }

  // Validate priority format
  if (priority_.empty() || priority_[0] != '<' || priority_.find('>') == std::string::npos) {
    throw std::runtime_error("Invalid priority format: " + priority_);
  }

  // Parse structured data field (can be "-" or "[...]")
  std::string remaining_line;
  std::getline(stream, remaining_line);
  
  // Trim leading space
  if (!remaining_line.empty() && remaining_line[0] == ' ') {
    remaining_line = remaining_line.substr(1);
  }
  
  if (remaining_line.empty()) {
    throw std::runtime_error("Missing structured data and message fields");
  }
  
  if (remaining_line[0] == '-') {
    // NILVALUE case
    hyphen_ = "-";
    if (remaining_line.size() > 1 && remaining_line[1] == ' ') {
      message_content = remaining_line.substr(2);
    } else if (remaining_line.size() == 1) {
      message_content = "";
    } else {
      throw std::runtime_error("Invalid structured data format");
    }
  } else if (remaining_line[0] == '[') {
    // Structured data case - find matching closing bracket
    size_t bracket_count = 0;
    size_t sd_end = 0;
    
    for (size_t i = 0; i < remaining_line.size(); ++i) {
      if (remaining_line[i] == '[') {
        bracket_count++;
      } else if (remaining_line[i] == ']') {
        bracket_count--;
        if (bracket_count == 0) {
          sd_end = i + 1;
          break;
        }
      }
    }
    
    if (bracket_count != 0) {
      throw std::runtime_error("Unmatched brackets in structured data");
    }
    
    hyphen_ = remaining_line.substr(0, sd_end);
    
    // Extract message content after structured data
    if (sd_end < remaining_line.size()) {
      if (remaining_line[sd_end] == ' ') {
        message_content = remaining_line.substr(sd_end + 1);
      } else {
        message_content = remaining_line.substr(sd_end);
      }
    } else {
      message_content = "";
    }
  } else {
    throw std::runtime_error("Invalid structured data format: must start with '-' or '['");
  }

  // Normalize message content to UTF-8 and remove BOM
  std::string normalized_msg = normalizeToUTF8(message_content);
  bom_size_ = static_cast<unsigned short>(message_content.size() - normalized_msg.size());

  return std::move(normalized_msg);
}

void SyslogServerThread::handleClient() {
  char buffer[16 * 1024];
  int rx_len;
  size_t message_length = 0;
  size_t processed_size = 0;
  bool parsing_error = false;
  std::string accumulated_message; // Buffer to accumulate complete message
  bool message_parsed = false; // Flag to track if message headers have been parsed
  
  // process the message length metadata
  while ((rx_len = SSL_read(ssl_, buffer, static_cast<int>(sizeof(buffer) - 1))) > 0) {
    // Ensure null termination
    buffer[rx_len] = '\0';
    
    if (processed_size == 0) {
      // Reset parsing state for new message
      parsing_error = false;
      message_parsed = false;
      bom_size_ = 0;
      accumulated_message.clear();
      
      // Find the space that separates length from payload
      std::string buffer_str(buffer);
      size_t space_pos = buffer_str.find(' ');
      
      if (space_pos == std::string::npos) {
        std::cerr << "Invalid message format: no space separator found" << std::endl;
        parsing_error = true;
        continue;
      }
      
      size_t data_start_index = space_pos + 1;
      
      // Extract and validate message length
      std::string message_length_str = buffer_str.substr(0, space_pos);
      try {
        message_length = std::stoull(message_length_str);
        if (message_length == 0 || message_length > 1024 * 1024) { // Reasonable limits
          std::cerr << "Invalid message length: " << message_length << std::endl;
          parsing_error = true;
          continue;
        }
      } catch (const std::exception& e) {
        std::cerr << "Failed to parse message length: " << e.what() << std::endl;
        parsing_error = true;
        continue;
      }
      
      // Validate data_start_index bounds
      if (data_start_index >= static_cast<size_t>(rx_len)) {
        std::cerr << "Invalid data start index" << std::endl;
        parsing_error = true;
        continue;
      }
      
      // Start accumulating the message data
      accumulated_message.append(buffer + data_start_index, rx_len - data_start_index);
      processed_size = accumulated_message.size();
      
    } else {
      // Continue accumulating message content
      if (!parsing_error) {
        accumulated_message.append(buffer, rx_len);
        processed_size = accumulated_message.size();
      }
    }
    
    // Check if we have received the complete message
    if (!parsing_error && processed_size >= message_length) {
      // Ensure we don't process more than the expected message length
      if (processed_size > message_length) {
        accumulated_message.resize(message_length);
      }
      
      // Now parse the complete syslog message
      try {
        std::string message_content = parseSyslogMsg(accumulated_message.c_str());
        
        // Validate that parsing was successful
        if (priority_.empty() || timestamp_.empty() || hostname_.empty()) {
          std::cerr << "Failed to parse syslog header fields" << std::endl;
          parsing_error = true;
          processed_size = 0;
          continue;
        }
        
        int priorityDigit = extractPriorityDigit(priority_);
        logger_ptr_->startColorLine(priorityDigit);
        
        // Process header and message content
        logger_ptr_->processMessage(
            priority_ + std::string(" ") +
            timestamp_ + std::string(" ") +
            hostname_ + std::string(" ") +
            app_name_ + std::string(" ") +
            process_id_ + std::string(" ") +
            message_id_ + std::string(" ") +
            hyphen_ + std::string(" "));
        logger_ptr_->processMessage(message_content);
        
        logger_ptr_->endLine(); // Message processing complete
        
      } catch (const std::exception& e) {
        std::cerr << "Error parsing syslog message: " << e.what() << std::endl;
        parsing_error = true;
      }
      
      // Reset state for next message
      processed_size = 0;
      priority_.clear();
      timestamp_.clear();
      hostname_.clear();
      app_name_.clear();
      process_id_.clear();
      message_id_.clear();
      hyphen_.clear();
      accumulated_message.clear();
      
      // If we received more data than expected, we need to handle the overflow
      if (rx_len > static_cast<int>(message_length - (processed_size - rx_len))) {
        // This shouldn't happen with proper protocol implementation,
        // but we should handle it gracefully
        std::cerr << "Warning: Received more data than expected message length" << std::endl;
      }
    }
  }
  
  // Handle SSL errors and cleanup partial state
  if (rx_len != 0) { // 0 is clean disconnect
    int ssl_err = SSL_get_error(ssl_, rx_len);
    auto err_err = ERR_get_error();
    if (ssl_err == SSL_ERROR_SYSCALL) {
      if (err_err != 0) // 0 is most probably an unexpected timeout/disconnect
        std::cerr << "Socket I/O error" << std::endl;
    } else {
      std::cerr << "SSL " << ERR_error_string(err_err, NULL) << std::endl;
    }
    
    // Reset state on error
    processed_size = 0;
    priority_.clear();
    timestamp_.clear();
    hostname_.clear();
    app_name_.clear();
    process_id_.clear();
    message_id_.clear();
    hyphen_.clear();
  }
}

void SyslogServerThread::clientCleanup() {
  if (ssl_) {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
  if (client_socket_ != -1) {
    std::cout << "Client disconnected: " << client_ip_ << std::endl;
    closesocket(client_socket_);
    client_socket_ = -1;
  }
}

void SyslogServerThread::run() {
  if (SSL_accept(ssl_) == 1) {
    handleClient();
  }
  // Cleanup the client connection
  clientCleanup();
}
