/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "testRunner.hpp"
#include "arguments.h"
#include "os.h"

TEST_CASE(Parse_all_mode_no_override) {
    Arguments args;
    char argument[] = "start,all,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 0);
    ASSERT_EQ(args._nativemem, DEFAULT_ALLOC_INTERVAL);
    ASSERT_EQ(args._lock, DEFAULT_LOCK_INTERVAL);
    ASSERT_EQ(args._nativelock, DEFAULT_LOCK_INTERVAL);
    ASSERT_EQ(args._live, true);
    const char* expected_event = OS::isLinux() ? EVENT_CPU : NULL;
    ASSERT_EQ(args._event, expected_event);
    ASSERT_EQ(args._proc, OS::isLinux() ? DEFAULT_PROC_INTERVAL : -1);
}

TEST_CASE(Parse_all_mode_event_override) {
    Arguments args;
    char argument[] = "start,all,event=cycles,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 0);
    ASSERT_EQ(args._nativemem, DEFAULT_ALLOC_INTERVAL);
    ASSERT_EQ(args._lock, DEFAULT_LOCK_INTERVAL);
    ASSERT_EQ(args._nativelock, DEFAULT_LOCK_INTERVAL);
    ASSERT_EQ(args._live, true);
    ASSERT_EQ(args._proc, OS::isLinux() ? DEFAULT_PROC_INTERVAL : -1);
}

TEST_CASE(Parse_all_mode_event_and_threshold_override) {
    Arguments args;
    char argument[] = "start,all,event=cycles,nativemem=10,lock=100,alloc=1000,wall=10000,proc=10,nativelock=100000,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 10000);
    ASSERT_EQ(args._alloc, 1000);
    ASSERT_EQ(args._nativemem, 10);
    ASSERT_EQ(args._lock, 100);
    ASSERT_EQ(args._nativelock, 100000);
    ASSERT_EQ(args._live, true);
    ASSERT_EQ(args._proc, 10);
}

TEST_CASE(Parse_override_before_all_mode) {
    Arguments args;
    char argument[] = "start,event=cycles,nativemem=10,lock=100,alloc=1000,proc=10,nativelock=1000,all,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 1000);
    ASSERT_EQ(args._nativemem, 10);
    ASSERT_EQ(args._lock, 100);
    ASSERT_EQ(args._nativelock, 1000);
    ASSERT_EQ(args._live, true);
    ASSERT_EQ(args._proc, 10);
}

TEST_CASE(Parse_proc_standalone) {
    Arguments args;
    char argument[] = "start,proc=5,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._proc, 5);
    ASSERT_EQ(args._all, false);
}

TEST_CASE(Parse_proc_default_value) {
    Arguments args;
    char argument[] = "start,proc,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._proc, DEFAULT_PROC_INTERVAL);
}

TEST_CASE(Parse_proc_with_units) {
    Arguments args;
    char argument[] = "start,proc=2m,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._proc, 120);
}

TEST_CASE(Parse_ratelimit) {
    Arguments args;
    char argument[] = "start,event=cpu,ratelimit=cpu:1000;alloc:200;span:100,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(error.message(), NULL);
    ASSERT_EQ(args._rate_limit[EC_CPU], 1000);
    ASSERT_EQ(args._rate_limit[EC_ALLOC], 200);
    ASSERT_EQ(args._rate_limit[EC_SPAN], 100);
    for (int i = 0; i < EC_CATEGORIES; i++) {
        if (i != EC_CPU && i != EC_ALLOC && i != EC_SPAN) {
            ASSERT_EQ(args._rate_limit[i], -1);
        }
    }
}

TEST_CASE(Parse_ratelimit_all_categories) {
    Arguments args;
    char argument[] = "start,ratelimit=cpu:1;alloc:2;lock:3;wall:4;nativemem:5;nativelock:6;trace:7;span:0,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(error.message(), NULL);
    ASSERT_EQ(args._rate_limit[EC_CPU], 1);
    ASSERT_EQ(args._rate_limit[EC_ALLOC], 2);
    ASSERT_EQ(args._rate_limit[EC_LOCK], 3);
    ASSERT_EQ(args._rate_limit[EC_WALL], 4);
    ASSERT_EQ(args._rate_limit[EC_NATIVEMEM], 5);
    ASSERT_EQ(args._rate_limit[EC_NATIVELOCK], 6);
    ASSERT_EQ(args._rate_limit[EC_TRACE], 7);
    ASSERT_EQ(args._rate_limit[EC_SPAN], 0);
}

TEST_CASE(Parse_ratelimit_invalid) {
    const char* invalid_arguments[] = {
        "start,ratelimit,file=%f.jfr",
        "start,ratelimit=cpu,file=%f.jfr",
        "start,ratelimit=cpu:,file=%f.jfr",
        "start,ratelimit=cpu:-5,file=%f.jfr",
        "start,ratelimit=foo:100,file=%f.jfr",
        "start,ratelimit=cpu:100;;lock:200,file=%f.jfr",
        "start,ratelimit=cpu:100@lock:200,file=%f.jfr",
    };
    for (size_t i = 0; i < sizeof(invalid_arguments) / sizeof(invalid_arguments[0]); i++) {
        Arguments args;
        Error error = args.parse(invalid_arguments[i]);
        ASSERT_NE(error.message(), NULL);
        ASSERT_EQ(strcmp(error.message(), "Invalid ratelimit"), 0);
    }
}
