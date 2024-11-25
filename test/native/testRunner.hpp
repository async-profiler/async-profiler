/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TESTRUNNER_HPP
#define _TESTRUNNER_HPP

#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct TestCase;

class TestRunner {
  private:
    std::map<std::string, TestCase> _test_cases;

    TestRunner(const TestRunner&) = delete;
    TestRunner& operator=(const TestRunner&) = delete;

  public:
    TestRunner() : _test_cases() {
    }

    static TestRunner* instance();

    inline std::map<std::string, TestCase>& testCases() {
        return _test_cases;
    }

    int runAllTests();
};

struct TestCase {
    std::string name;
    std::function<void()> test_function;
    bool only; // only run this test when true, ignore all others.
    std::string filename;
    int line_no;
    int assertion_count = 0;
    bool has_failed_assertions = false;
    bool skipped = false;

    TestCase(const std::string& name, std::function<void()> test_function, bool only, const std::string& filename, int line_no)
        : name(name), test_function(test_function), only(only), filename(filename), line_no(line_no) {}
};

#define ASSERT(condition) ASSERT_NE(condition, NULL)
#define ASSERT_FALSE(condition) ASSERT_EQ(condition, NULL)
#define CHECK(condition) CHECK_NE(condition, NULL)
#define CHECK_FALSE(condition) CHECK_EQ(condition, NULL)

#define ASSERT_OP(val1, op, val2) __ASSERT_OR_CHECK_OP(true, val1, op, val2)

#define CHECK_OP(val1, op, val2) __ASSERT_OR_CHECK_OP(false, val1, op, val2)

#define __ASSERTED(isAssert)                \
    test_case.has_failed_assertions = true; \
    if (isAssert) {                         \
        return;                             \
    }

#define __ASSERT_OR_CHECK_OP(isAssert, val1, op, val2)                                                               \
    {                                                                                                                \
        const bool is_string =                                                                                       \
            std::is_same<decltype(val1), const char*>::value || std::is_same<decltype(val2), const char*>::value;    \
        if (is_string) {                                                                                             \
            if ((std::string(#op) == "==") || (std::string(#op) == "!=")) {                                          \
                const char* str1 = reinterpret_cast<const char*>(val1);                                              \
                const char* str2 = reinterpret_cast<const char*>(val2);                                              \
                if ((std::string(#op) == "==" && (str1 && str2 && strcmp(str1, str2) != 0)) ||                       \
                    (std::string(#op) == "!=" && (str1 == str2 || (str1 && str2 && strcmp(str1, str2) == 0)))) {     \
                    printf("Assertion failed: (%s %s %s),\n\tactual values: %s = \"%s\", %s = \"%s\"\n\tat %s:%d\n", \
                           #val1, #op, #val2, #val1, str1, #val2, str2, __FILE__, __LINE__);                         \
                    __ASSERTED(isAssert)                                                                             \
                }                                                                                                    \
            } else {                                                                                                 \
                printf("Invalid assertion %s, strings can only be compared with == or !=.\n\tat %s:%d\n", #op,       \
                       __FILE__, __LINE__);                                                                          \
                test_case.has_failed_assertions = true;                                                              \
                return;                                                                                              \
            }                                                                                                        \
        } else if (!((val1)op(val2))) {                                                                              \
            printf("Assertion failed: (%s %s %s),\n\tactual values: %s = %lld (0x%llX), %s = %lld (0x%llX)\n\tat "   \
                   "%s:%d\n",                                                                                        \
                   #val1, #op, #val2, #val1, (u64)(val1), (u64)(val1), #val2, (u64)(val2), (u64)(val2), __FILE__,    \
                   __LINE__);                                                                                        \
            __ASSERTED(isAssert)                                                                                     \
        } else {                                                                                                     \
            test_case.assertion_count++;                                                                             \
        }                                                                                                            \
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

#define __TEST_CASE(test_name, precondition, only)                                                        \
    void test_name(TestCase& test_case);                                                                  \
    void test_name##_runner();                                                                            \
    static TestRegistrar test_name##_registrar(#test_name, test_name##_runner, only, __FILE__, __LINE__); \
    void test_name##_runner() {                                                                           \
        TestCase& test_case = TestRunner::instance()->testCases().at(#test_name);                         \
        test_case.assertion_count = 0;                                                                    \
        if (!(precondition)) {                                                                            \
            test_case.skipped = true;                                                                     \
            return;                                                                                       \
        }                                                                                                 \
        test_name(test_case);                                                                             \
        if (!test_case.has_failed_assertions && test_case.assertion_count == 0) {                         \
            printf("%s: No assertions were made.\n", #test_name);                                         \
        }                                                                                                 \
        return;                                                                                           \
    }                                                                                                     \
    void test_name(TestCase& test_case)

#define __SELECT_IMPL(_1, _2, NAME, ...) NAME
#define TEST_CASE(...) __SELECT_IMPL(__VA_ARGS__, __TEST_CASE2, __TEST_CASE1)(__VA_ARGS__)
#define ONLY_TEST_CASE(...) __SELECT_IMPL(__VA_ARGS__, __ONLY_TEST_CASE2, __ONLY_TEST_CASE1)(__VA_ARGS__)

#define __TEST_CASE1(test_name) __TEST_CASE(test_name, true, false)
#define __TEST_CASE2(test_name, precondition) __TEST_CASE(test_name, precondition, false)

#define __ONLY_TEST_CASE1(test_name) __TEST_CASE(test_name, true, true)
#define __ONLY_TEST_CASE2(test_name, precondition) __TEST_CASE(test_name, precondition, true)

struct TestRegistrar {
    TestRegistrar(const std::string& name, std::function<void()> test_function, bool only, const std::string& filename,
                  int line_no) {
        TestRunner::instance()->testCases().emplace(name, TestCase(name, test_function, only, filename, line_no));
    }
};

bool fileReadable(const char* filename);

#endif // _TESTRUNNER_HPP
