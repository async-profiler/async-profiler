/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_runner.hpp"

int main()
{
    return TestRunner::instance()->runAllTests();
}
