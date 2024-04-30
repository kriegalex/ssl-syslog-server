#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <random>

#include <windows.h>
#include <psapi.h>

#include "Logger.h"

void printMemoryUsage() {
  PROCESS_MEMORY_COUNTERS_EX pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &pmc, sizeof(pmc))) {
    size_t physMemUsedByMe = pmc.WorkingSetSize;
    std::cout << "Current physical memory usage: " << physMemUsedByMe << " bytes" << std::endl;
  } else {
    std::cerr << "Failed to get memory info." << std::endl;
  }
}

// Function to generate random test strings
std::vector<std::string> generateTestStrings(size_t numberOfStrings) {
  std::vector<std::string> strings;
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> distribution(166, 167); // Range for the second random integer

  for (size_t i = 0; i < numberOfStrings; ++i) {
    int number = distribution(generator);
    std::string str = " <" + std::to_string(number) +
        "> very long long long long long long long long long long long long long long long message";
    std::string msgSize = std::to_string(str.size());
    str = msgSize + str;
    strings.push_back(str);
  }
  return strings;
}

// Benchmark using manual loop
auto benchmark(const std::vector<std::string> &strings) {
  auto start = std::chrono::high_resolution_clock::now();

  // Perform benchmarks
  Config config("config.json");
  Logger logger(config);
  for (const auto &str : strings) {
    logger.processMessage(str.c_str());
  }

  auto end = std::chrono::high_resolution_clock::now();
  logger.stopWaitLoggers();
  return end - start;
}

int main() {
  size_t numberOfStrings = 8000000; // Number of test strings to generate

  // Generate test strings according to the specified format
  std::vector<std::string> testStrings = generateTestStrings(numberOfStrings);

  printMemoryUsage();

  auto start = std::chrono::high_resolution_clock::now();
  auto findDuration = benchmark(testStrings);
  auto end = std::chrono::high_resolution_clock::now();

  // Output results
  std::cout << "Benchmarking with " << numberOfStrings << " strings." << std::endl;
  std::cout << "Processing duration: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(findDuration).count()
            << " milliseconds." << std::endl;
  std::cout << "I/O Processing duration: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
            << " milliseconds." << std::endl;

  return 0;
}
