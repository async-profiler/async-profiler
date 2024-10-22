#ifndef _TEST_RUNNER_HPP
#define _TEST_RUNNER_HPP

#include <vector>
#include <functional>
#include <string>
#include <cstdio>

struct TestCase;

class TestRunner
{
private:
    static TestRunner* _instance;

    std::vector<TestCase> _testCases;

    TestRunner(const TestRunner &) = delete;
    TestRunner &operator=(const TestRunner &) = delete;

public:
    TestRunner() : _testCases() {}

    static TestRunner* instance();

    inline std::vector<TestCase> &testCases()
    {
        return _testCases;
    }

    void runAllTests();
};

struct TestCase
{
    std::string name;
    std::function<void()> testFunction;
    int assertionCount = 0;
    bool hasFailedAssertions = false;
    bool skipped = false;
};

#define ASSERT_MSG(condition, message)                        \
    if (!(condition))                                         \
    {                                                         \
        fprintf(stderr, "Assertion failed: %s\n\tat %s:%d\n", \
                #condition, __FILE__, __LINE__);              \
        if (message)                                          \
        {                                                     \
            fprintf(stderr, "\t%s\n", message);               \
        }                                                     \
        testCase.hasFailedAssertions = true;                  \
        return;                                               \
    }                                                         \
    else                                                      \
    {                                                         \
        testCase.assertionCount++;                            \
    }

#define ASSERT(condition) ASSERT_MSG(condition, nullptr)

#define ASSERT_FALSE(condition) ASSERT(!(condition))

#define ASSERT_OP(val1, op, val2) \
    ASSERT((val1)op(val2))

#define ASSERT_EQ(val1, val2) ASSERT_OP(val1, ==, val2)
#define ASSERT_NE(val1, val2) ASSERT_OP(val1, !=, val2)
#define ASSERT_GT(val1, val2) ASSERT_OP(val1, >, val2)
#define ASSERT_GTE(val1, val2) ASSERT_OP(val1, >=, val2)
#define ASSERT_LT(val1, val2) ASSERT_OP(val1, <, val2)
#define ASSERT_LTE(val1, val2) ASSERT_OP(val1, <=, val2)

#define __TEST_CASE(testName, skipCondition)                               \
    void testName(TestCase &testCase);                                     \
    void testName##Runner();                                               \
    static TestRegistrar testName##Registrar(#testName, testName##Runner); \
    void testName##Runner()                                                \
    {                                                                      \
        TestCase &testCase = TestRunner::instance()->testCases().back();   \
        testCase.assertionCount = 0;                                       \
        if (skipCondition)                                                 \
        {                                                                  \
            testCase.skipped = true;                                       \
            return;                                                        \
        }                                                                  \
        testName(testCase);                                                \
        if (!testCase.hasFailedAssertions && testCase.assertionCount == 0) \
        {                                                                  \
            fprintf(stderr, "%s: No assertions were made.\n", #testName);  \
        }                                                                  \
        return;                                                            \
    }                                                                      \
    void testName(TestCase &testCase)

#define SKIPPABLE_TEST_CASE(testName, skipCondition) __TEST_CASE(testName, skipCondition)

#define TEST_CASE(testName) __TEST_CASE(testName, false)

struct TestRegistrar
{
    TestRegistrar(const std::string &name, std::function<void()> testFunction)
    {
        TestRunner::instance()->testCases().push_back({name, testFunction});
    }
};

void runAllTests();

#endif // _TEST_RUNNER_HPP
