/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CALLTRACESTORAGE_H
#define _CALLTRACESTORAGE_H

#include <map>
#include <vector>
#include "arch.h"
#include "linearAllocator.h"
#include "vmEntry.h"


class LongHashTable;

struct CallTrace {
    int num_frames;
    ASGCT_CallFrame frames[1];
};

struct CallTraceSample {
    CallTrace* trace;
    u64 samples;
    u64 counter;

    CallTrace* acquireTrace() {
        return __atomic_load_n(&trace, __ATOMIC_ACQUIRE);
    }

    void setTrace(CallTrace* value) {
        return __atomic_store_n(&trace, value, __ATOMIC_RELEASE);
    }

    CallTraceSample& operator+=(const CallTraceSample& s) {
        trace = s.trace;
        samples += s.samples;
        counter += s.counter;
        return *this;
    }
};

class CallTraceStorage {
  private:
    static CallTrace _overflow_trace;

    LinearAllocator _allocator;
    LongHashTable* _current_table;
    u64 _overflow;

    u64 calcHash(int num_frames, ASGCT_CallFrame* frames);
    CallTrace* storeCallTrace(int num_frames, ASGCT_CallFrame* frames);
    CallTrace* findCallTrace(LongHashTable* table, u64 hash);

  public:
    CallTraceStorage();
    ~CallTraceStorage();

    void clear();
    size_t usedMemory();

    void collectTraces(std::map<u32, CallTrace*>& map);
    void collectSamples(std::vector<CallTraceSample*>& samples);
    void collectSamples(std::map<u64, CallTraceSample>& map);

    u32 put(int num_frames, ASGCT_CallFrame* frames, u64 counter);
    void add(u32 call_trace_id, u64 samples, u64 counter);
    void resetCounters();
};

#endif // _CALLTRACESTORAGE
