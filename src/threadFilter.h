/*
 * Copyright 2020 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

    bool accept(int thread_id);
    void add(int thread_id);
    void remove(int thread_id);

    void collect(std::vector<int>& v);
};

#endif // _THREADFILTER_H
