cmake_minimum_required(VERSION 3.15)
project(SecureSyslogServer)

# Set the C++ standard for the project
set(CMAKE_CXX_STANDARD 17)

set(OPENSSL_ROOT_DIR ON CACHE BOOL "OpenSSL root directory")

find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    message(STATUS "OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
    message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")
endif ()

# Create an executable with this project's source files
add_executable(${PROJECT_NAME} main.cpp
        SyslogServer.cpp
        Config.cpp
        SSLUtil.cpp
        Logger.cpp
        MemoryBoundedQueue.cpp)
target_link_libraries(SecureSyslogServer OpenSSL::SSL OpenSSL::Crypto)
if (WIN32)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif ()
target_include_directories(SecureSyslogServer PRIVATE ${OPENSSL_INCLUDE_DIR})

# Set the output directory for runtime binary (executables)
set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Copy a specific file
file(COPY ${PROJECT_SOURCE_DIR}/res/server.pem DESTINATION ${PROJECT_SOURCE_DIR}/bin/Debug)
file(COPY ${PROJECT_SOURCE_DIR}/res/server.pem DESTINATION ${PROJECT_SOURCE_DIR}/bin/Release)
file(COPY ${PROJECT_SOURCE_DIR}/res/config.json DESTINATION ${PROJECT_SOURCE_DIR}/bin/Debug)
file(COPY ${PROJECT_SOURCE_DIR}/res/config.json DESTINATION ${PROJECT_SOURCE_DIR}/bin/Release)