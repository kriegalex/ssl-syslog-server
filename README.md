# Simple C++ Syslog Server

This is a simple syslog server implemented in C++ that listens for encrypted SSL/TLS traffic. It requires a PEM file
containing both a certificate and a private key to establish a secure connection.

## Prerequisites

Before you run the syslog server, ensure that you have the following:

- OpenSSL library installed on your system. Tested on Windows with FireDaemon OpenSSL 3.
- C++ compiler with support for C++17 or later.
- For Windows, Visual Studio Build Tools installed.

## Installation

1. **Clone the Repository**
   ```bash
   git clone https://github.com/kriegalex/ssl-syslog-server.git
   cd ssl-syslog-server
   ```
2. **Compile the Server**
   
   Windows
   ```bash
   cmake.exe -G "Visual Studio 17 2022" . -B ./cmake-build
   cmake.exe --build ./cmake-build --target SecureSyslogServer --config Release
   ```
   Linux
   ```bash
   cmake . -B ./cmake-build
   cd cmake-build
   make -j 4
   ```
3. **Prepare the PEM File**
   - You must have a file named server.pem in the same directory as the executable. This file should contain your SSL certificate followed by the private key.
   - If you do not have a server.pem, you can generate one using OpenSSL:
   ```bash
   openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 365 -out certificate.pem
   cat certificate.pem key.pem > server.pem
   rm certificate.pem key.pem
   ```
## Usage
Run the server using the following command:
   ```bash
   ./syslog_server
   ```
The server will start and listen for incoming syslog messages over SSL/TLS on the specified port.

## Configuration
- Port Configuration: By default, the server listens on port 60119. If you wish to use a different port, you will need to modify the configuration file accordingly.
- SSL/TLS Configuration: The server is configured to use TLS v1.2 by default. Modifications in the SSL setup should be performed in the source code if different SSL/TLS standards or configurations are needed.

## Contributing
Contributions are welcome! Please feel free to submit pull requests or open issues to improve the functionality or efficiency of the syslog server.

## License
This project is licensed under the MIT License - see the LICENSE file for details.
