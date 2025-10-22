/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include "arguments.h"
#include "os.h"
#include "nativeLockTracer.h"

TEST_CASE(NativeLockTracer_start_with_valid_interval) {
    Arguments args;
    args._nativelock = 5000;
    
    NativeLockTracer tracer;
    Error error = tracer.start(args);
    
    ASSERT_EQ(error.message(), (const char*)NULL);
    ASSERT_EQ(NativeLockTracer::running(), true);
    
    tracer.stop();
    ASSERT_EQ(NativeLockTracer::running(), false);
}

TEST_CASE(NativeLockTracer_start_with_zero_interval) {
    Arguments args;
    args._nativelock = 0;
    
    NativeLockTracer tracer;
    Error error = tracer.start(args);
    
    ASSERT_EQ(error.message(), (const char*)NULL);
    ASSERT_EQ(NativeLockTracer::running(), true);
    
    tracer.stop();
}

TEST_CASE(NativeLockTracer_stop_sets_running_false) {
    Arguments args;
    args._nativelock = 1000;
    
    NativeLockTracer tracer;
    tracer.start(args);
    ASSERT_EQ(NativeLockTracer::running(), true);
    
    tracer.stop();
    ASSERT_EQ(NativeLockTracer::running(), false);
}
