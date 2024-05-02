#ifndef SSLUTIL_H
#define SSLUTIL_H

#include <openssl/ssl.h>
#include <string>

class SSLUtil {
 public:
  static SSL_CTX *createServerContext();
  static int createSocket(int port);
  static int acceptClient(int serverSocket);
  static void setupClient(int clientSocket);
  static std::string getClientIP(int clientSocket);
  static std::string sslErrorToString(int error);
  static SSL *createSSL(SSL_CTX *ctx, int clientSocket);
  static void initWinSocket();
  static void cleanWinSocket();

 private:
  static void configureContext(SSL_CTX *ctx);
};

#endif
