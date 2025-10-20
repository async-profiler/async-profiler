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

TEST_CASE(NativeLockTracer_start_with_negative_interval) {
    Arguments args;
    args._nativelock = -1;
    
    NativeLockTracer tracer;
    Error error = tracer.start(args);
    
    ASSERT_EQ(error.message(), (const char*)NULL);
    ASSERT_EQ(NativeLockTracer::running(), true);
    
    tracer.stop();
}

TEST_CASE(NativeLockTracer_type_returns_correct_string) {
    NativeLockTracer tracer;
    ASSERT_EQ(tracer.type(), "native_lock_tracer");
}

TEST_CASE(NativeLockTracer_title_returns_correct_string) {
    NativeLockTracer tracer;
    ASSERT_EQ(tracer.title(), "Native lock profile");
}

TEST_CASE(NativeLockTracer_units_returns_nanoseconds) {
    NativeLockTracer tracer;
    ASSERT_EQ(tracer.units(), "ns");
}

TEST_CASE(NativeLockTracer_running_false_initially) {
    ASSERT_EQ(NativeLockTracer::running(), false);
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
