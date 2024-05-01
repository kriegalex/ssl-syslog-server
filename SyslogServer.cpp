#include "SyslogServer.h"

#include <csignal>

// Initialize static instance pointer
SyslogServer *SyslogServer::instance_ = nullptr;

SyslogServer::SyslogServer(const std::string &configPath) : config_(configPath),
                                                            logger_(config_),
                                                            client_socket_(-1),
                                                            ssl_(nullptr) {
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
  if (instance_) {
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
  while (running_) {
    client_socket_ = SSLUtil::acceptClient(server_socket_);
    std::string client_ip = SSLUtil::getClientIP(client_socket_);
    // Now you have the client's IP
    std::cout << "Client connected: " << client_ip << std::endl;
    ssl_ = SSLUtil::createSSL(ssl_ctx_, client_socket_);
    if (SSL_accept(ssl_) == 1) {
      handleClient();
    }
    clientCleanup();
  }
}

void SyslogServer::handleClient() {
  char buffer[20 * 1024];
  while (int len = SSL_read(ssl_, buffer, static_cast<int>(sizeof(buffer) - 1))) {
    if (len > 0) {
      buffer[len] = '\0';
      logger_.processMessage(buffer);
    }
  }
}

void SyslogServer::cleanup() {
  std::cout << "Cleaning up sockets..." << std::endl;
  clientCleanup();
  serverCleanup();
}

void SyslogServer::clientCleanup() {
  if (ssl_) {
    SSL_shutdown(ssl_);
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
  if (client_socket_ != -1) {
    closesocket(client_socket_);
    client_socket_ = -1;
  }
}

void SyslogServer::serverCleanup() {
  if (server_socket_ != -1) {
    closesocket(server_socket_);
    server_socket_ = -1;
  }
}
