#ifndef SSLUTIL_H
#define SSLUTIL_H

#include <openssl/ssl.h>

class SSLUtil {
 public:
  static void cleanupOpenSSL();
  static SSL_CTX *createServerContext();
  static int createSocket(int port);
  static int acceptClient(int serverSocket);
  static SSL *createSSL(SSL_CTX *ctx, int clientSocket);
  static void initWinSocket();
  static void cleanWinSocket();

 private:
  static void configureContext(SSL_CTX *ctx);
};

#endif
