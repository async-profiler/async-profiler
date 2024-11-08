/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include "callTraceStorage.h"
#include "profiler.h"
#include "testRunner.hpp"
#include <dlfcn.h>
#include <stdio.h>

#define ASSERT_EVENT_TYPE(event_type, name_, type_, default_interval_, config_, config1_, config2_, counter_arg_) \
    ASSERT_NE(event_type, NULL);                                                                                  \
    CHECK_EQ(event_type->name, name_);                                                                            \
    CHECK_EQ(event_type->type, type_);                                                                            \
    CHECK_EQ(event_type->default_interval, default_interval_);                                                    \
    CHECK_EQ(event_type->config, config_);                                                                        \
    CHECK_EQ(event_type->config1, config1_);                                                                      \
    CHECK_EQ(event_type->config2, config2_);                                                                      \
    CHECK_EQ(event_type->counter_arg, counter_arg_)

#define ASSERT_EVENT_TYPE_NONZERO_CONFIG(event_type, name_, type_, default_interval_) \
    ASSERT_NE(event_type, NULL);                                                      \
    CHECK_EQ(event_type->name, name_);                                                \
    CHECK_EQ(event_type->type, type_);                                                \
    CHECK_EQ(event_type->default_interval, default_interval_);                        \
    CHECK_NE(event_type->config, 0);                                                  \
    CHECK_EQ(event_type->config1, 0);                                                 \
    CHECK_EQ(event_type->config2, 0);                                                 \
    CHECK_EQ(event_type->counter_arg, 0)

#define ASSERT_BP(event_type, bp_type, address, bp_len, counter_arg) \
    ASSERT_EVENT_TYPE(event_type, "mem:breakpoint", PERF_TYPE_BREAKPOINT, BKPT_INTERVAL, bp_type, address, bp_len, counter_arg)

#define ASSERT_RAW(event_type, name, config) ASSERT_EVENT_TYPE(event_type, name, PERF_TYPE_RAW, 1000, config, 0, 0, 0)

#define ASSERT_PROBE(event_type, name_, type_, default_interval_, config_, probeName, config2_, counter_arg_) \
    ASSERT_NE(event_type, NULL);                                                                              \
    CHECK_EQ(event_type->name, name_);                                                                        \
    CHECK_NE(type_, 0);                                                                                       \
    CHECK_EQ(event_type->default_interval, default_interval_);                                                \
    CHECK_EQ(event_type->config, config_);                                                                    \
    CHECK_EQ(((const char*)event_type->config1), probeName);                                                  \
    CHECK_EQ(event_type->config2, config2_);                                                                  \
    CHECK_EQ(event_type->counter_arg, counter_arg_)

TEST_CASE(ForName_Predefined_cpu) {
    PerfEventType* event_type = PerfEventType::forName("cpu");
    ASSERT_EVENT_TYPE(event_type, "cpu-clock", PERF_TYPE_SOFTWARE, DEFAULT_INTERVAL, PERF_COUNT_SW_CPU_CLOCK, 0, 0, 0);
}

TEST_CASE(ForName_Predefined_cpu_clock) {
    PerfEventType* event_type = PerfEventType::forName("cpu-clock");
    ASSERT_EVENT_TYPE(event_type, "cpu-clock", PERF_TYPE_SOFTWARE, DEFAULT_INTERVAL, PERF_COUNT_SW_CPU_CLOCK, 0, 0, 0);
}

TEST_CASE(ForName_Invalid_space) {
    PerfEventType* event_type = PerfEventType::forName(" ");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Invalid_Zero) {
    PerfEventType* event_type = PerfEventType::forName("mem:0");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Invalid) {
    PerfEventType* event_type = PerfEventType::forName("mem:foo_unknownSymbol");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_InvalidColon2) {
    PerfEventType* event_type = PerfEventType::forName("mem::");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_InvalidColon) {
    PerfEventType* event_type = PerfEventType::forName("mem:");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_HexAddr) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x123, 1, 0);
}

TEST_CASE(ForName_Breakpoint_DecAddr) {
    PerfEventType* event_type = PerfEventType::forName("mem:123");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Addr_DecOffset) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+22000");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 22291, 1, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 1, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_DecLen) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/8");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 8, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 0x8, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_R) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:r");
    ASSERT_BP(event_type, HW_BREAKPOINT_R, 0x22123, 0x8, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_W) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:w");
    ASSERT_BP(event_type, HW_BREAKPOINT_W, 0x22123, 0x8, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X) {
    char name[50];
    snprintf(name, sizeof(name), "mem:0x123+0x22000/0x%lx:x", sizeof(long));

    PerfEventType* event_type = PerfEventType::forName(name);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, 0x22123, sizeof(long), 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_InvalidDefaultRW) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:i");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X_Arg) {
    char name[50];
    snprintf(name, sizeof(name), "mem:0x123+0x22000/0x%lx:x{3}", sizeof(long));

    PerfEventType* event_type = PerfEventType::forName(name);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, 0x22123, sizeof(long), 3);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_RW) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:rw");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 0x8, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_RX) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:rx");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_WX) {
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x8:wx");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X_Known) {
    char name[50];
    snprintf(name, sizeof(name), "mem:mmap+0x22000/0x%lx:x", sizeof(long));
    PerfEventType* event_type = PerfEventType::forName(name);
    const __u64 mmap_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "mmap");

    ASSERT_NE(mmap_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, mmap_addr + 0x22000, sizeof(long), 2);
}

TEST_CASE(ForName_Breakpoint_Symbol) {
    PerfEventType* event_type = PerfEventType::forName("mem:strcmp");
    const __u64 strcmp_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "strcmp");

    ASSERT_NE(strcmp_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, strcmp_addr, 1, 0);
}

TEST_CASE(ForName_Tracepoint_Invalid) {
    PerfEventType* event_type = PerfEventType::forName("trace:foo");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Tracepoint_Id) {
    PerfEventType* event_type = PerfEventType::forName("trace:123");
    ASSERT_EVENT_TYPE(event_type, "trace:tracepoint", PERF_TYPE_TRACEPOINT, 1, 123, 0, 0, 0);
}

TEST_CASE(ForName_Tracepoint_Zero) {
    PerfEventType* event_type = PerfEventType::forName("trace:0");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_kprobe_Invalid) {
    PerfEventType* event_type = PerfEventType::forName("kprobe:");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_kprobe, fileReadable("/sys/bus/event_source/devices/kprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("kprobe:foo");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 0, "foo", 0, 0);
}

TEST_CASE(ForName_kprobe_offset, fileReadable("/sys/bus/event_source/devices/kprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("kprobe:foo+99");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 0, "foo", 99, 0);
}

TEST_CASE(ForName_uprobe, fileReadable("/sys/bus/event_source/devices/uprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("uprobe:foo");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 0, "foo", 0, 0);
}

TEST_CASE(ForName_uprobe_offset, fileReadable("/sys/bus/event_source/devices/uprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("uprobe:foo+0x99");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 0, "foo", 0x99, 0);
}

TEST_CASE(ForName_kretprobe, fileReadable("/sys/bus/event_source/devices/kprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("kretprobe:foo");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 1, "foo", 0, 0);
}

TEST_CASE(ForName_kretprobe_offset, fileReadable("/sys/bus/event_source/devices/kprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("kretprobe:foo+-10");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 1, "foo", -10, 0);
}

TEST_CASE(ForName_uretprobe, fileReadable("/sys/bus/event_source/devices/uprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("uretprobe:foo");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 1, "foo", 0, 0);
}

TEST_CASE(ForName_uretprobe_offset, fileReadable("/sys/bus/event_source/devices/uprobe/type")) {
    PerfEventType* event_type = PerfEventType::forName("uretprobe:foo+066");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 1, "foo", 54, 0);
}

TEST_CASE(ForName_raw_pmu_0) {
    PerfEventType* event_type = PerfEventType::forName("r0");
    ASSERT_RAW(event_type, "rNNN", 0);
}

TEST_CASE(ForName_raw_pmu_33) {
    PerfEventType* event_type = PerfEventType::forName("r33");
    ASSERT_RAW(event_type, "rNNN", 0x33);
}

TEST_CASE(ForName_raw_pmu_033) {
    PerfEventType* event_type = PerfEventType::forName("r033");
    ASSERT_RAW(event_type, "rNNN", 0x33);
}

TEST_CASE(ForName_raw_pmu_u64) {
    PerfEventType* event_type = PerfEventType::forName("rF00FFFFF0123999");
    ASSERT_RAW(event_type, "rNNN", 0xF00FFFFF0123999);
}

TEST_CASE(ForName_symbol) {
    PerfEventType* event_type = PerfEventType::forName("strcmp");
    const __u64 strcmp_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "strcmp");

    ASSERT_NE(strcmp_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, strcmp_addr, sizeof(long), 0);
}

TEST_CASE(ForName_symbol_private) {
    Profiler::instance()->updateSymbols(false);
    PerfEventType* event_type = PerfEventType::forName("asprof_execute");

    const __u64 addr = (__u64)(uintptr_t)asprof_execute;
    ASSERT_BP(event_type, HW_BREAKPOINT_X, addr, sizeof(long), 0);

    const __u64 dyn_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "asprof_execute");
    // This symbol is not a dynamic symbol.
    ASSERT_EQ(dyn_addr, 0);
}

TEST_CASE(ForName_pmu_descriptor, fileReadable("/sys/bus/event_source/devices/cpu/events/cache-misses")) {
    PerfEventType* event_type = PerfEventType::forName("cpu/cache-misses/");
    ASSERT_EVENT_TYPE_NONZERO_CONFIG(event_type, "pmu/event-descriptor/", PERF_TYPE_RAW, 1000);
}

TEST_CASE(ForName_kernel_tracepoint, fileReadable("/sys/kernel/tracing/events/oom/mark_victim/id") ||
                                         fileReadable("/sys/kernel/debug/tracing/events/oom/mark_victim/id")) {
    PerfEventType* event_type = PerfEventType::forName("oom:mark_victim");
    ASSERT_EVENT_TYPE_NONZERO_CONFIG(event_type, "trace:tracepoint", PERF_TYPE_TRACEPOINT, 1);
}

#endif // __linux__
