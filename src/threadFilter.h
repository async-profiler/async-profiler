/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _THREADFILTER_H
#define _THREADFILTER_H

#include <vector>
#include "arch.h"


// The size of thread ID bitmap in bytes. Must be at least 64K to allow mmap()
const u32 BITMAP_SIZE = 65536;
// How many thread IDs one bitmap can hold
const u32 BITMAP_CAPACITY = BITMAP_SIZE * 8;
// Total number of bitmaps required to hold the entire range of thread IDs
const u32 MAX_BITMAPS = (1 << 31) / BITMAP_CAPACITY;


// ThreadFilter query operations must be lock-free and signal-safe;
// update operations are mostly lock-free, except rare bitmap allocations
class ThreadFilter {
  private:
    u32* _bitmap[MAX_BITMAPS];
    bool _enabled;
    volatile int _size;

    u32* bitmap(int thread_id) {
        return _bitmap[(u32)thread_id / BITMAP_CAPACITY];
    }

    u32& word(u32* bitmap, int thread_id) {
        return bitmap[((u32)thread_id % BITMAP_CAPACITY) >> 5];
    }

  public:
    ThreadFilter();
    ~ThreadFilter();

    bool enabled() {
        return _enabled;
    }

    int size() {
        return _size;
    }

    void init(const char* filter);
    void clear();

    size_t usedMemory();

    bool accept(int thread_id);
    void add(int thread_id);
    void remove(int thread_id);

    void collect(std::vector<int>& v);
};

#endif // _THREADFILTER_H
