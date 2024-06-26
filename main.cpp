#include "SyslogServer.h"
#include <iostream>

int main(int argc, char **argv) {
  try {
    SyslogServer server("config.json");
    server.run();
  }
  catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
