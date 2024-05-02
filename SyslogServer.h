#pragma once

#include <openssl/ssl.h>

#include "Config.h"
#include "SSLUtil.h"
#include "Logger.h"

class SyslogServerThread {
 public:
  SyslogServerThread(SSL *ssl,
                     int client_socket,
                     std::string client_ip,
                     std::shared_ptr<Logger> logger_ptr);
  void run();
  void clientCleanup();

 private:
  SSL *ssl_;
  int client_socket_;
  std::string client_ip_;
  std::shared_ptr<Logger> logger_ptr_;

  void handleClient();
};

class SyslogServer {
 public:
  explicit SyslogServer(const std::string &configPath);
  ~SyslogServer();
  void run();
  void cleanup();

 private:
  Config config_;
  std::shared_ptr<Logger> logger_ptr_;
  SSL_CTX *ssl_ctx_{};
  int server_socket_;
  std::vector<std::weak_ptr<SyslogServerThread>> threads_;
  std::mutex shutdown_mutex_;
  bool running_{};

  static SyslogServer *instance_;
  static void shutdownServer(int sig);

  static void setupSignals();
  static void enableVirtualTerminalProcessing();
  void acceptConnections();
  void serverCleanup();
};
