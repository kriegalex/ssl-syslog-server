#pragma once

#include "Config.h"
#include <string>
#include <iostream>
#include <openssl/ssl.h>

#include "SSLUtil.h"
#include "Logger.h"

class SyslogServer {
 public:
  explicit SyslogServer(const std::string &configPath);
  ~SyslogServer();
  void run();
  void cleanup();

 private:
  Config config_;
  Logger logger_;
  SSL_CTX *ssl_ctx_;
  int server_socket_;
  int client_socket_;
  SSL *ssl_;
  bool running_{};

  static SyslogServer *instance_;
  static void shutdownServer(int sig);

  static void setupSignals();
  static void enableVirtualTerminalProcessing();
  void acceptConnections();
  void handleClient();
  void clientCleanup();
  void serverCleanup();
};
