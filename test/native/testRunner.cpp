/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "os.h"
#include "testRunner.hpp"
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

bool fileReadable(const char* filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    char buf[1];
    ssize_t r = read(fd, buf, sizeof(buf));
    close(fd);
    return r != -1;
}

int TestRunner::runAllTests() {
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int total_assertions = 0;
    int i = 1;
    double total_duration = 0;
    const int total_tests = testCases().size();

    const bool redirected = !isatty(STDOUT_FILENO);
    const char* red = redirected ? "" : "\033[31m";
    const char* green = redirected ? "" : "\033[32m";
    const char* yellow = redirected ? "" : "\033[33m";
    const char* reset = redirected ? "" : "\033[0m";

    bool has_only = false;
    for (auto& pair : testCases()) {
        has_only |= pair.second.only;
    }
    if (has_only) {
        printf("Only running tests marked with ONLY_TEST_CASE.\n");
    }

    for (auto& pair : testCases()) {
        auto& test_case = pair.second;
        bool force_skip = has_only && !test_case.only;

        printf("Running %s @ %s:%d\n", test_case.name.c_str(), test_case.filename.c_str(), test_case.line_no);

        u64 start = OS::nanotime();
        if (!force_skip) {
            test_case.test_function();
        } else {
            test_case.skipped = true;
        }
        double duration = (OS::nanotime() - start) / 1e6;

        total_duration += duration;
        total_assertions += test_case.assertion_count;
        if (test_case.skipped) {
            printf("%sSKIP%s [%d/%d] %s took %0.1f ms\n", yellow, reset, i, total_tests, test_case.name.c_str(), duration);
            skipped++;
        } else if (test_case.has_failed_assertions) {
            printf("%sFAIL%s [%d/%d] %s took %0.1f ms\n", red, reset, i, total_tests, test_case.name.c_str(), duration);
            failed++;
        } else {
            printf("%sPASS%s [%d/%d] %s took %0.1f ms\n", green, reset, i, total_tests, test_case.name.c_str(), duration);
            passed++;
        }
        i++;
    }

    printf("\nTotal C++ test duration: %0.1f ms\n", total_duration);
    printf("Total successful assertions: %d\n", total_assertions);
    printf("PASS: %d (%0.1f%%)\n", passed, 100.0 * (passed + skipped) / total_tests);
    printf("FAIL: %d\n", failed);
    printf("SKIP: %d\n", skipped);
    printf("TOTAL: %d\n", total_tests);

    return failed > 0 ? 1 : 0;
}
