#include "testRunner.hpp"
#include "arguments.h"

TEST_CASE(Parse_all_mode_no_override) {
    Arguments args;
    char argument[] = "start,all,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 0);
    ASSERT_EQ(args._nativemem, 0);
    ASSERT_EQ(args._lock, DEFAULT_LOCK_INTERVAL);
    ASSERT_EQ(args._live, true);
    #ifdef __linux__
    ASSERT_EQ(args._event, EVENT_CPU);
    #endif
    #ifdef __APPLE__
    ASSERT_EQ(args._event, NULL);
    #endif
}

TEST_CASE(Parse_all_mode_event_override) {
    Arguments args;
    char argument[] = "start,all,event=cycles,file=%f.jfr";
    Error error = args.parse(argument);
    ASSERT_EQ(args._all, true);
    ASSERT_EQ(args._event, "cycles");
    ASSERT_EQ(args._wall, 0);
    ASSERT_EQ(args._alloc, 0);
    ASSERT_EQ(args._nativemem, 0);
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