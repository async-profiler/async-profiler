/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TEST_RUNNER_HPP
#define _TEST_RUNNER_HPP

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct TestCase;

class TestRunner
{
  private:
    static TestRunner* _instance;

    std::map<std::string, TestCase> _testCases;

    TestRunner(const TestRunner&) = delete;
    TestRunner& operator=(const TestRunner&) = delete;

  public:
    TestRunner() : _testCases()
    {
    }

    static TestRunner* instance();

    inline std::map<std::string, TestCase>& testCases()
    {
        return _testCases;
    }

    int runAllTests();
};

struct TestCase
{
    std::string name;
    std::function<void()> testFunction;
    bool only; // only run this test when true, ignore all others.
    std::string filename;
    int lineno;
    int assertionCount = 0;
    bool hasFailedAssertions = false;
    bool skipped = false;
};

#define ASSERT(condition) ASSERT_NE(condition, NULL)
#define ASSERT_FALSE(condition) ASSERT_EQ(condition, NULL)
#define CHECK(condition) CHECK_NE(condition, NULL)
#define CHECK_FALSE(condition) CHECK_EQ(condition, NULL)

#define ASSERT_OP(val1, op, val2) __ASSERT_OR_CHECK_OP(true, val1, op, val2)

#define CHECK_OP(val1, op, val2) __ASSERT_OR_CHECK_OP(false, val1, op, val2)

#define __ASSERTED(isAssert)                                                                                           \
    testCase.hasFailedAssertions = true;                                                                               \
    if (isAssert)                                                                                                      \
    {                                                                                                                  \
        return;                                                                                                        \
    }

#define __ASSERT_OR_CHECK_OP(isAssert, val1, op, val2)                                                                 \
    {                                                                                                                  \
        const bool isString =                                                                                          \
            std::is_same<decltype(val1), const char*>::value || std::is_same<decltype(val2), const char*>::value;      \
        if (isString)                                                                                                  \
        {                                                                                                              \
            if ((std::string(#op) == "==") || (std::string(#op) == "!="))                                              \
            {                                                                                                          \
                const char* str1 = reinterpret_cast<const char*>(val1);                                                \
                const char* str2 = reinterpret_cast<const char*>(val2);                                                \
                if ((std::string(#op) == "==" && (str1 && str2 && strcmp(str1, str2) != 0)) ||                         \
                    (std::string(#op) == "!=" && (str1 == str2 || (str1 && str2 && strcmp(str1, str2) == 0))))         \
                {                                                                                                      \
                    printf("Assertion failed: (%s %s %s),\n\tactual values: %s = \"%s\", %s = \"%s\"\n\tat %s:%d\n",   \
                           #val1, #op, #val2, #val1, (val1), #val2, (val2), __FILE__, __LINE__);                       \
                    __ASSERTED(isAssert)                                                                               \
                }                                                                                                      \
            }                                                                                                          \
            else                                                                                                       \
            {                                                                                                          \
                printf("Invalid assertion %s, strings can only be compared with == or !=.\n\tat %s:%s\n", #op,         \
                       __FILE__, __LINE__);                                                                            \
                testCase.hasFailedAssertions = true;                                                                   \
                return;                                                                                                \
            }                                                                                                          \
        }                                                                                                              \
        else if (!((val1)op(val2)))                                                                                    \
        {                                                                                                              \
            printf("Assertion failed: (%s %s %s),\n\tactual values: %s = %lld (0x%llX), %s = %lld (0x%llX)\n\tat "     \
                   "%s:%d\n",                                                                                          \
                   #val1, #op, #val2, #val1, (__u64)(val1), (__u64)(val1), #val2, (__u64)(val2), (__u64)(val2),        \
                   __FILE__, __LINE__);                                                                                \
            __ASSERTED(isAssert)                                                                                       \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            testCase.assertionCount++;                                                                                 \
        }                                                                                                              \
    }

// ASSERT stops execution after a failure.
#define ASSERT_EQ(val1, val2) ASSERT_OP(val1, ==, val2)
#define ASSERT_NE(val1, val2) ASSERT_OP(val1, !=, val2)
#define ASSERT_GT(val1, val2) ASSERT_OP(val1, >, val2)
#define ASSERT_GTE(val1, val2) ASSERT_OP(val1, >=, val2)
#define ASSERT_LT(val1, val2) ASSERT_OP(val1, <, val2)
#define ASSERT_LTE(val1, val2) ASSERT_OP(val1, <=, val2)

// CHECK continues execution after a failure.
#define CHECK_EQ(val1, val2) CHECK_OP(val1, ==, val2)
#define CHECK_NE(val1, val2) CHECK_OP(val1, !=, val2)
#define CHECK_GT(val1, val2) CHECK_OP(val1, >, val2)
#define CHECK_GTE(val1, val2) CHECK_OP(val1, >=, val2)
#define CHECK_LT(val1, val2) CHECK_OP(val1, <, val2)
#define CHECK_LTE(val1, val2) CHECK_OP(val1, <=, val2)

#define __TEST_CASE(testName, skip, only)                                                                              \
    void testName(TestCase& testCase);                                                                                 \
    void testName##Runner();                                                                                           \
    static TestRegistrar testName##Registrar(#testName, testName##Runner, only, __FILE__, __LINE__);                   \
    void testName##Runner()                                                                                            \
    {                                                                                                                  \
        TestCase& testCase = TestRunner::instance()->testCases().at(#testName);                                        \
        testCase.assertionCount = 0;                                                                                   \
        if (skip)                                                                                                      \
        {                                                                                                              \
            testCase.skipped = true;                                                                                   \
            return;                                                                                                    \
        }                                                                                                              \
        testName(testCase);                                                                                            \
        if (!testCase.hasFailedAssertions && testCase.assertionCount == 0)                                             \
        {                                                                                                              \
            printf("%s: No assertions were made.\n", #testName);                                                       \
        }                                                                                                              \
        return;                                                                                                        \
    }                                                                                                                  \
    void testName(TestCase& testCase)

#define __SELECT_IMPL(_1, _2, NAME, ...) NAME
#define TEST_CASE(...) __SELECT_IMPL(__VA_ARGS__, __TEST_CASE2, __TEST_CASE1)(__VA_ARGS__)
#define ONLY_TEST_CASE(...) __SELECT_IMPL(__VA_ARGS__, __TEST_CASE2, __TEST_CASE1)(__VA_ARGS__)

#define __TEST_CASE1(testName) __TEST_CASE(testName, false, false)
#define __TEST_CASE2(testName, skip) __TEST_CASE(testName, skip, false)

#define __ONLY_TEST_CASE1(testName) __TEST_CASE(testName, false, true)
#define __ONLY_TEST_CASE2(testName, skip) __TEST_CASE(testName, skip, true)

struct TestRegistrar
{
    TestRegistrar(const std::string& name, std::function<void()> testFunction, bool only, const std::string& filename,
                  int lineno)
    {
        TestRunner::instance()->testCases()[name] = {name, testFunction, only, filename, lineno};
    }
};

bool fileReadable(const char* file_name);

#endif // _TEST_RUNNER_HPP
