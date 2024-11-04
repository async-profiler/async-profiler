/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include <chrono>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
  return TestRunner::instance()->runAllTests();
}

TestRunner* TestRunner::instance() {
  static TestRunner instance;
  return &instance;
}

bool fileReadable(const char* file_name) {
  int fd = open(file_name, O_RDONLY);
  if (fd == -1) {
    return false;
  }

  char buf[1];
  ssize_t r = read(fd, buf, sizeof(buf));
  close(fd);
  return r != -1;
}

int TestRunner::runAllTests() {
  int passedTests = 0;
  int failedTests = 0;
  int skippedTests = 0;
  int totalAssertions = 0;
  int i = 1;
  double totalDuration = 0;
  const int totalTests = testCases().size();

  const bool redirected = !isatty(fileno(stdout));
  const char* red = redirected ? "" : "\033[31m";
  const char* green = redirected ? "" : "\033[32m";
  const char* yellow = redirected ? "" : "\033[33m";
  const char* reset = redirected ? "" : "\033[0m";

  bool hasOnly = false;
  for (auto& pair : testCases()) {
    hasOnly |= pair.second.only;
  }
  if (hasOnly) {
    printf("Only running tests marked with ONLY_TEST_CASE.\n");
  }

  for (auto& pair : testCases()) {
    auto& testCase = pair.second;
    bool forceSkip = hasOnly && !testCase.only;

    printf("Running %s @ %s:%d\n", testCase.name.c_str(), testCase.filename.c_str(), testCase.lineno);

    std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
    if (!forceSkip) {
      testCase.testFunction();
    } else {
      testCase.skipped = true;
    }
    std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - start;

    totalDuration += duration.count();
    totalAssertions += testCase.assertionCount;
    if (testCase.skipped) {
      printf("%sSKIP%s [%d/%d] %s took %0.1f ms\n", yellow, reset, i, totalTests, testCase.name.c_str(), duration.count());
      skippedTests++;
    } else if (testCase.hasFailedAssertions) {
      printf("%sFAIL%s [%d/%d] %s took %0.1f ms\n", red, reset, i, totalTests, testCase.name.c_str(), duration.count());
      failedTests++;
    } else {
      printf("%sPASS%s [%d/%d] %s took %0.1f ms\n", green, reset, i, totalTests, testCase.name.c_str(), duration.count());
      passedTests++;
    }
    i++;
  }

  printf("\nTotal C++ test duration: %0.1f ms\n", totalDuration);
  printf("Total successful assertions: %d\n", totalAssertions);
  printf("PASS: %d (%0.1f%%)\n", passedTests, 100.0 * passedTests / totalTests);
  printf("FAIL: %d\n", failedTests);
  printf("SKIP: %d\n", skippedTests);
  printf("TOTAL: %d\n", totalTests);

  return failedTests > 0 ? 1 : 0;
}
