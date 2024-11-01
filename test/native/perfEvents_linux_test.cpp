/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include "callTraceStorage.h"
#include "perfEvents.h"
#include "test_runner.hpp"
#include <linux/perf_event.h>
#include <stdio.h>

#define ASSERT_EVENT_TYPE(event_type, name_, type_, default_interval_, config_, config1_, config2_, counter_arg_)      \
    ASSERT_NE(event_type, NULL);                                                                                       \
    CHECK_EQ(event_type->name, name_);                                                                                 \
    CHECK_EQ(event_type->type, type_);                                                                                 \
    CHECK_EQ(event_type->default_interval, default_interval_);                                                         \
    CHECK_EQ(event_type->config, config_);                                                                             \
    CHECK_EQ(event_type->config1, config1_);                                                                           \
    CHECK_EQ(event_type->config2, config2_);                                                                           \
    CHECK_EQ(event_type->counter_arg, counter_arg_)

#define ASSERT_BP(event_type, bp_type, address, bp_len, counter_arg)                                                   \
    ASSERT_EVENT_TYPE(event_type, "mem:breakpoint", PERF_TYPE_BREAKPOINT, 1, bp_type, address, bp_len, counter_arg)

#define ASSERT_RAW(event_type, name, config) ASSERT_EVENT_TYPE(event_type, name, PERF_TYPE_RAW, 1000, config, 0, 0, 0)

#define ASSERT_PROBE(event_type, name_, type_, default_interval_, config_, probeName, config2_, counter_arg_)          \
    ASSERT_NE(event_type, NULL);                                                                                       \
    CHECK_EQ(event_type->name, name_);                                                                                 \
    CHECK_NE(type_, 0);                                                                                                \
    CHECK_EQ(event_type->default_interval, default_interval_);                                                         \
    CHECK_EQ(event_type->config, config_);                                                                             \
    CHECK_EQ(((const char*)event_type->config1), probeName);                                                           \
    CHECK_EQ(event_type->config2, config2_);                                                                           \
    CHECK_EQ(event_type->counter_arg, counter_arg_)

TEST_CASE(ForName_Predefined)
{
    PerfEventType* event_type = PerfEventType::forName("cpu");
    ASSERT_EVENT_TYPE(event_type, "cpu", PERF_TYPE_SOFTWARE, DEFAULT_INTERVAL, PERF_COUNT_SW_CPU_CLOCK, 0, 0, 0);
}

TEST_CASE(ForName_Invalid_space)
{
    PerfEventType* event_type = PerfEventType::forName(" ");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Invalid_Zero)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Invalid)
{
    PerfEventType* event_type = PerfEventType::forName("mem:foo_unknownSymbol");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_InvalidColon2)
{
    PerfEventType* event_type = PerfEventType::forName("mem::");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_InvalidColon)
{
    PerfEventType* event_type = PerfEventType::forName("mem:");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_HexAddr)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x123, 1, 0);
}

TEST_CASE(ForName_Breakpoint_DecAddr)
{
    PerfEventType* event_type = PerfEventType::forName("mem:123");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Breakpoint_Addr_DecOffset)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+22000");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 22291, 1, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 1, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_DecLen)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/16");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 0x16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_R)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16:r");
    ASSERT_BP(event_type, HW_BREAKPOINT_R, 0x22123, 0x16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_W)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16:w");
    ASSERT_BP(event_type, HW_BREAKPOINT_W, 0x22123, 0x16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16:x");
    ASSERT_BP(event_type, HW_BREAKPOINT_X, 0x22123, 0x16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_InvalidDefaultRW)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16:i");
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, 0x22123, 0x16, 0);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X_Arg)
{
    PerfEventType* event_type = PerfEventType::forName("mem:0x123+0x22000/0x16:x{3}");
    ASSERT_BP(event_type, HW_BREAKPOINT_X, 0x22123, 0x16, 3);
}

TEST_CASE(ForName_Breakpoint_Addr_HexOffset_HexLen_X_Known)
{
    const __u64 mmap_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "mmap");
    PerfEventType* event_type = PerfEventType::forName("mem:mmap+0x22000/0x16:x");

    ASSERT_NE(mmap_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, mmap_addr + 0x22000, 0x16, 2);
}

TEST_CASE(ForName_Breakpoint_Symbol)
{
    PerfEventType* event_type = PerfEventType::forName("mem:strcmp");
    const __u64 strcmp_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "strcmp");

    ASSERT_NE(strcmp_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_RW, strcmp_addr, 1, 0);
}

TEST_CASE(ForName_Tracepoint_Invalid)
{
    PerfEventType* event_type = PerfEventType::forName("trace:foo");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_Tracepoint_Id)
{
    PerfEventType* event_type = PerfEventType::forName("trace:123");
    ASSERT_EVENT_TYPE(event_type, "trace:tracepoint", PERF_TYPE_TRACEPOINT, 1, 123, 0, 0, 0);
}

TEST_CASE(ForName_Tracepoint_Zero)
{
    PerfEventType* event_type = PerfEventType::forName("trace:0");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_kprobe_Invalid)
{
    PerfEventType* event_type = PerfEventType::forName("kprobe:");
    ASSERT_EQ(event_type, NULL);
}

TEST_CASE(ForName_kprobe)
{
    PerfEventType* event_type = PerfEventType::forName("kprobe:foo");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 0, "foo", 0, 0);
}

TEST_CASE(ForName_kprobe_offset)
{
    PerfEventType* event_type = PerfEventType::forName("kprobe:foo+99");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 0, "foo", 99, 0);
}

TEST_CASE(ForName_uprobe)
{
    PerfEventType* event_type = PerfEventType::forName("uprobe:foo");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 0, "foo", 0, 0);
}

TEST_CASE(ForName_uprobe_offset)
{
    PerfEventType* event_type = PerfEventType::forName("uprobe:foo+0x99");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 0, "foo", 0x99, 0);
}

TEST_CASE(ForName_kretprobe)
{
    PerfEventType* event_type = PerfEventType::forName("kretprobe:foo");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 1, "foo", 0, 0);
}

TEST_CASE(ForName_kretprobe_offset)
{
    PerfEventType* event_type = PerfEventType::forName("kretprobe:foo+-10");
    ASSERT_PROBE(event_type, "kprobe:func", 6, 1, 1, "foo", -10, 0);
}

TEST_CASE(ForName_uretprobe)
{
    PerfEventType* event_type = PerfEventType::forName("uretprobe:foo");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 1, "foo", 0, 0);
}

TEST_CASE(ForName_uretprobe_offset)
{
    PerfEventType* event_type = PerfEventType::forName("uretprobe:foo+066");
    ASSERT_PROBE(event_type, "uprobe:path", 7, 1, 1, "foo", 54, 0);
}

TEST_CASE(ForName_raw_pmu_0)
{
    PerfEventType* event_type = PerfEventType::forName("r0");
    ASSERT_RAW(event_type, "rNNN", 0);
}

TEST_CASE(ForName_raw_pmu_33)
{
    PerfEventType* event_type = PerfEventType::forName("r33");
    ASSERT_RAW(event_type, "rNNN", 0x33);
}

TEST_CASE(ForName_raw_pmu_033)
{
    PerfEventType* event_type = PerfEventType::forName("r033");
    ASSERT_RAW(event_type, "rNNN", 0x33);
}

TEST_CASE(ForName_raw_pmu_u64)
{
    PerfEventType* event_type = PerfEventType::forName("rF00FFFFF0123999");
    ASSERT_RAW(event_type, "rNNN", 0xF00FFFFF0123999);
}

TEST_CASE(ForName_symbol)
{
    PerfEventType* event_type = PerfEventType::forName("strcmp");
    const __u64 strcmp_addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, "strcmp");

    ASSERT_NE(strcmp_addr, 0);
    ASSERT_BP(event_type, HW_BREAKPOINT_X, strcmp_addr, 8, 0);
}

TEST_CASE(ForName_pmu_descriptor, fileReadable("/sys/bus/event_source/devices/cpu/events/cache-misses"))
{
    PerfEventType* event_type = PerfEventType::forName("cpu/cache-misses/");
    ASSERT_RAW(event_type, "pmu/event-descriptor/", 0x412E);
}

TEST_CASE(ForName_kernel_tracepoint, fileReadable("/sys/kernel/tracing/events/oom/mark_victim/id") ||
                                         fileReadable("/sys/kernel/debug/tracing/events/oom/mark_victim/id"))
{
    PerfEventType* event_type = PerfEventType::forName("oom:mark_victim");
    ASSERT_EVENT_TYPE(event_type, "trace:tracepoint", PERF_TYPE_TRACEPOINT, 1, 0x1E7, 0, 0, 0);
}

#endif // __linux__
