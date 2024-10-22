#include "test_runner.hpp"
#include <chrono>

TestRunner* TestRunner::_instance = NULL;

TestRunner* TestRunner::instance()
{
    if (!_instance)
    {
        _instance = new TestRunner();
    }

    return _instance;
}

void TestRunner::runAllTests()
{
    int passedTests = 0;
    int failedTests = 0;
    int skippedTests = 0;
    int totalAssertions = 0;
    int i = 1;
    long totalDuration = 0;
    const int totalTests = testCases().size();

    for (auto &testCase : testCases())
    {
        printf("Running %s\n", testCase.name.c_str());

        auto start = std::chrono::high_resolution_clock::now();
        testCase.testFunction();
        std::chrono::duration<double, std::milli> duration = std::chrono::high_resolution_clock::now() - start;
        totalDuration += duration.count();
        totalAssertions += testCase.assertionCount;
        if (testCase.skipped)
        {
            printf("SKIP [%d/%d] %s\n", i, totalTests, testCase.name.c_str());
            skippedTests++;
        }
        else if (testCase.hasFailedAssertions)
        {
            printf("FAIL [%d/%d] %s\n", i, totalTests, testCase.name.c_str());
            failedTests++;
        }
        else
        {
            printf("PASS [%d/%d] %s\n", i, totalTests, testCase.name.c_str());
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
}
