#include "SyslogServer.h"

#include <csignal>
#include <utility>
#include <openssl/err.h>

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
    if(client_socket != INVALID_SOCKET) {
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
  for(const auto& weak_thread: threads_) {
    if(auto thread = weak_thread.lock()) {
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

void SyslogServerThread::handleClient() {
  char buffer[20 * 1024] = {0};
  int rx_len;
  while ((rx_len = SSL_read(ssl_, buffer, static_cast<int>(sizeof(buffer) - 1))) > 0) {
    buffer[rx_len] = '\0';
    logger_ptr_->processMessage(buffer);
  }
  if(rx_len != 0) { // 0 is clean disconnect
    int ssl_err = SSL_get_error(ssl_,rx_len);
    auto err_err = ERR_get_error();
    if(ssl_err == SSL_ERROR_SYSCALL) {
      if(err_err != 0) // 0 is most probably an unexpected timeout/disconnect
        std::cerr << "Socket I/O error" << std::endl;
    } else {
      std::cerr << "SSL " << ERR_error_string(err_err, NULL) << std::endl;
    }
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
