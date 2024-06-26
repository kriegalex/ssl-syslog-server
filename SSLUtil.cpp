#include "SSLUtil.h"

#include <iostream>
#include <stdexcept>
#include <openssl/err.h>
#ifdef OPENSSL_SYS_WINDOWS
#include <winsock2.h>
#include <ip2string.h>
#endif

SSL_CTX *SSLUtil::createServerContext() {
  const SSL_METHOD *method = TLS_server_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) {
    throw std::runtime_error("Unable to create SSL context.");
  }
  configureContext(ctx);
  return ctx;
}

void SSLUtil::configureContext(SSL_CTX *ctx) {
  if (SSL_CTX_use_certificate_chain_file(ctx, "server.pem") <= 0) {
    ERR_print_errors_fp(stderr);
    throw std::runtime_error("Failed to load certificate.");
  }

  if (SSL_CTX_use_PrivateKey_file(ctx, "server.pem", SSL_FILETYPE_PEM) <= 0) {
    ERR_print_errors_fp(stderr);
    throw std::runtime_error("Failed to load private key.");
  }
}

void SSLUtil::initWinSocket() {
  WSADATA wsaData;
  int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (res != NO_ERROR) {
    throw std::runtime_error("Error at WSAStartup.");
  }
}

void SSLUtil::cleanWinSocket() {
  WSACleanup();
}

int SSLUtil::createSocket(int port) {
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    throw std::runtime_error("Unable to create socket.");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  int optval = 1;
  // Reuse the address; good for quick restarts
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof(optval)) < 0) {
    throw std::runtime_error("Unable to set socket option SO_REUSEADDR.");
  }

  if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    throw std::runtime_error("Unable to bind to socket.");
  }

  if (listen(s, 1) < 0) {
    throw std::runtime_error("Unable to listen to socket.");
  }

  return s;
}

std::string SSLUtil::getClientIP(int clientSocket) {
  struct sockaddr_in client_addr{};
  int addr_len = sizeof(client_addr);

  // Retrieve client information
  if (getpeername(clientSocket, (struct sockaddr *) &client_addr, &addr_len) == 0) {
    char client_ip[16];
    RtlIpv4AddressToStringA(&client_addr.sin_addr, client_ip);
    return std::move(std::string(client_ip));
  }
  return std::move(std::string("Unknown"));
}

int SSLUtil::acceptClient(int serverSocket) {
  sockaddr_in addr{};
  int len = sizeof(addr);
  int client = accept(serverSocket, (struct sockaddr *) &addr, &len);
  if (client == INVALID_SOCKET) {
    if (WSAGetLastError() != WSAEINTR) // the server was probably killed intentionally
      throw std::runtime_error("Unable to accept client.");
  }
  return client;
}

SSL *SSLUtil::createSSL(SSL_CTX *ctx, int clientSocket) {
  SSL *ssl = SSL_new(ctx);
  if (!ssl)
    throw std::runtime_error("Unable to create SSL structure.");

  if (!SSL_set_fd(ssl, clientSocket))
    throw std::runtime_error("Unable to set the SSL file descriptor.");
  return ssl;
}

void SSLUtil::setupClient(int clientSocket) {
  int timeout = 60000; // Timeout in milliseconds, 60 sec
  if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof(timeout)) < 0) {
    throw std::runtime_error("Unable to set socket option SO_RCVTIMEO.");
  }
  int optval = 1;
  // Set TCP_NODELAY for syslog performance tuning
  if (setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char *) &optval, sizeof(optval)) < 0) {
    throw std::runtime_error("Unable to set socket option TCP_NODELAY.");
  }
}
