/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PERFEVENTS_LINUX_HPP
#define _PERFEVENTS_LINUX_HPP

#ifdef __linux__

#include "perfEvents.h"
#include <linux/perf_event.h>
#include <stdint.h>

enum {
    HW_BREAKPOINT_R  = 1,
    HW_BREAKPOINT_W  = 2,
    HW_BREAKPOINT_RW = 3,
    HW_BREAKPOINT_X  = 4
};

struct FunctionWithCounter {
    const char* name;
    int counter_arg;
};

struct PerfEventType {
    const char* name;
    long default_interval;
    __u32 type;
    __u64 config;
    __u64 config1;
    __u64 config2;
    int counter_arg;

    enum {
        IDX_CPU = 0,
        IDX_PREDEFINED = 12,
        IDX_RAW,
        IDX_PMU,
        IDX_BREAKPOINT,
        IDX_TRACEPOINT,
        IDX_KPROBE,
        IDX_UPROBE,
    };

    static PerfEventType AVAILABLE_EVENTS[];
    static FunctionWithCounter KNOWN_FUNCTIONS[];
    static char probe_func[256];

    static int findCounterArg(const char* name);
    static PerfEventType* getBreakpoint(const char* name, __u32 bp_type, __u32 bp_len);
    static PerfEventType* getTracepoint(int tracepoint_id);
    static PerfEventType* getProbe(PerfEventType* probe, const char* type, const char* name, __u64 ret);
    static PerfEventType* getRawEvent(__u64 config);
    static PerfEventType* getPmuEvent(const char* name);
    static PerfEventType* forName(const char* name);
};

// Hardware breakpoint with interval=1 causes an infinite loop on ARM64
#ifdef __aarch64__
#  define BKPT_INTERVAL 2
#else
#  define BKPT_INTERVAL 1
#endif

#endif // __linux__

#endif // _PERFEVENTS_LINUX_HPP