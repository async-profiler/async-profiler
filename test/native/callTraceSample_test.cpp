/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "callTraceStorage.h"
#include "test_runner.hpp"

TEST_CASE(CallTraceSampleOps)
{
    CallTrace trace;

    CallTraceSample left;
    left.trace = NULL;
    left.samples = 99;
    left.counter = 11;

    CallTraceSample right;
    right.trace = &trace;
    right.samples = 900;
    right.counter = 100;

    ASSERT(right > left);

    left += right;
    ASSERT(left.samples == 999);
    ASSERT(left.counter == 111);
    ASSERT(left.trace == &trace);

    ASSERT(left > right);
}
