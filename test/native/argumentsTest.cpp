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
    ASSERT_EQ(args._live, true);
    const char* expected_event = OS::isLinux() ? EVENT_CPU : NULL;
    ASSERT_EQ(args._event, expected_event);
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
    ASSERT_EQ(args._live, true);
}

TEST_CASE(Parse_all_mode_event_and_threshold_override) {
    Arguments args;
    char argument[] = "start,all,event=cycles,nativemem=10,lock=100,alloc=1000,wall=10000,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 10000);
    ASSERT_EQ(args._alloc, 1000);
    ASSERT_EQ(args._nativemem, 10);
    ASSERT_EQ(args._lock, 100);
    ASSERT_EQ(args._live, true);
}

TEST_CASE(Parse_override_before_all_mode) {
    Arguments args;
    char argument[] = "start,event=cycles,nativemem=10,lock=100,alloc=1000,all,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 1000);
    ASSERT_EQ(args._nativemem, 10);
    ASSERT_EQ(args._lock, 100);
    ASSERT_EQ(args._live, true);
}

#ifndef __linux__
TEST_CASE(Parse_proc_on_non_linux_should_fail) {
    Arguments args;
    char argument[] = "start,event=proc,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ((bool)error, true);
    ASSERT_EQ(strcmp(error.message(), "Process sampling is only supported on Linux"), 0);
}

TEST_CASE(Parse_proc_with_interval_on_non_linux_should_fail) {
    Arguments args;
    char argument[] = "start,proc=30,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ((bool)error, true);
    ASSERT_EQ(strcmp(error.message(), "Process sampling is only supported on Linux"), 0);
}
#endif

#ifdef __linux__
TEST_CASE(Parse_proc_on_linux_should_succeed) {
    Arguments args;
    char argument[] = "start,event=proc,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ((bool)error, false);
    ASSERT_EQ(args._proc, DEFAULT_PROC_INTERVAL);
}

TEST_CASE(Parse_proc_with_interval_on_linux_should_succeed) {
    Arguments args;
    char argument[] = "start,proc=60,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ((bool)error, false);
    ASSERT_EQ(args._proc, 60);
}
#endif
