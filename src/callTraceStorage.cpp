/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "callTraceStorage.h"
#include "os.h"


static const u32 INITIAL_CAPACITY = 65536;
static const u32 CALL_TRACE_CHUNK = 8 * 1024 * 1024;
static const u32 OVERFLOW_TRACE_ID = 0x7fffffff;


class LongHashTable {
  private:
    LongHashTable* _prev;
    void* _padding0;
    u32 _capacity;
    u32 _padding1[15];
    volatile u32 _size;
    u32 _padding2[15];

    static size_t getSize(u32 capacity) {
        size_t size = sizeof(LongHashTable) + (sizeof(u64) + sizeof(CallTraceSample)) * capacity;
        return (size + OS::page_mask) & ~OS::page_mask;
    }

  public:
    static LongHashTable* allocate(LongHashTable* prev, u32 capacity) {
        LongHashTable* table = (LongHashTable*)OS::safeAlloc(getSize(capacity));
        if (table != NULL) {
            table->_prev = prev;
            table->_capacity = capacity;
            table->_size = 0;
        }
        return table;
    }

    LongHashTable* destroy() {
        LongHashTable* prev = _prev;
        OS::safeFree(this, getSize(_capacity));
        return prev;
    }

    size_t usedMemory() {
        return getSize(_capacity);
    }

    LongHashTable* prev() {
        return _prev;
    }

    u32 capacity() {
        return _capacity;
    }

    u32 size() {
        return _size;
    }

    u32 incSize() {
        return __sync_add_and_fetch(&_size, 1);
    }

    u64* keys() {
        return (u64*)(this + 1);
    }

    CallTraceSample* values() {
        return (CallTraceSample*)(keys() + _capacity);
    }

    void clear() {
        memset(keys(), 0, (sizeof(u64) + sizeof(CallTraceSample)) * _capacity);
        _size = 0;
    }
};


CallTrace CallTraceStorage::_overflow_trace = {1, {BCI_ERROR, (jmethodID)"storage_overflow"}};

CallTraceStorage::CallTraceStorage() : _allocator(CALL_TRACE_CHUNK) {
    _current_table = LongHashTable::allocate(NULL, INITIAL_CAPACITY);
    _overflow = 0;
}

CallTraceStorage::~CallTraceStorage() {
    while (_current_table != NULL) {
        _current_table = _current_table->destroy();
    }
}

void CallTraceStorage::clear() {
    while (_current_table->prev() != NULL) {
        _current_table = _current_table->destroy();
    }
    _current_table->clear();
    _allocator.clear();
    _overflow = 0;
}

size_t CallTraceStorage::usedMemory() {
    size_t bytes = _allocator.usedMemory();
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        bytes += table->usedMemory();
    }
    return bytes;
}

void CallTraceStorage::collectTraces(std::map<u32, CallTrace*>& map) {
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        u64* keys = table->keys();
        CallTraceSample* values = table->values();
        u32 capacity = table->capacity();

        for (u32 slot = 0; slot < capacity; slot++) {
            if (keys[slot] != 0 && loadAcquire(values[slot].samples) != 0) {
                // Reset samples to avoid duplication of call traces between JFR chunks
                values[slot].samples = 0;
                CallTrace* trace = values[slot].acquireTrace();
                if (trace != NULL) {
                    map[capacity - (INITIAL_CAPACITY - 1) + slot] = trace;
                }
            }
        }
    }

    if (_overflow > 0) {
        map[OVERFLOW_TRACE_ID] = &_overflow_trace;
    }
}

void CallTraceStorage::collectSamples(std::vector<CallTraceSample*>& samples) {
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        u64* keys = table->keys();
        CallTraceSample* values = table->values();
        u32 capacity = table->capacity();

        for (u32 slot = 0; slot < capacity; slot++) {
            if (keys[slot] != 0) {
                samples.push_back(&values[slot]);
            }
        }
    }
}

void CallTraceStorage::collectSamples(std::map<u64, CallTraceSample>& map) {
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        u64* keys = table->keys();
        CallTraceSample* values = table->values();
        u32 capacity = table->capacity();

        for (u32 slot = 0; slot < capacity; slot++) {
            if (keys[slot] != 0 && values[slot].acquireTrace() != NULL) {
                map[keys[slot]] += values[slot];
            }
        }
    }
}

// Adaptation of MurmurHash64A by Austin Appleby
u64 CallTraceStorage::calcHash(int num_frames, ASGCT_CallFrame* frames) {
    const u64 M = 0xc6a4a7935bd1e995ULL;
    const int R = 47;

    int len = num_frames * sizeof(ASGCT_CallFrame);
    u64 h = len * M;

    const u64* data = (const u64*)frames;
    const u64* end = data + len / 8;

    while (data != end) {
        u64 k = *data++;
        k *= M;
        k ^= k >> R;
        k *= M;
        h ^= k;
        h *= M;
    }

    if (len & 4) {
        h ^= *(u32*)data;
        h *= M;
    }

    h ^= h >> R;
    h *= M;
    h ^= h >> R;

    return h;
}

CallTrace* CallTraceStorage::storeCallTrace(int num_frames, ASGCT_CallFrame* frames) {
    const size_t header_size = sizeof(CallTrace) - sizeof(ASGCT_CallFrame);
    CallTrace* buf = (CallTrace*)_allocator.alloc(header_size + num_frames * sizeof(ASGCT_CallFrame));
    if (buf != NULL) {
        buf->num_frames = num_frames;
        // Do not use memcpy inside signal handler
        for (int i = 0; i < num_frames; i++) {
            buf->frames[i] = frames[i];
        }
    }
    return buf;
}

CallTrace* CallTraceStorage::findCallTrace(LongHashTable* table, u64 hash) {
    u64* keys = table->keys();
    u32 capacity = table->capacity();
    u32 slot = hash & (capacity - 1);
    u32 step = 0;

    while (keys[slot] != hash) {
        if (keys[slot] == 0) {
            return NULL;
        }
        if (++step >= capacity) {
            return NULL;
        }
        slot = (slot + step) & (capacity - 1);
    }

    return table->values()[slot].trace;
}

u32 CallTraceStorage::put(int num_frames, ASGCT_CallFrame* frames, u64 counter) {
    u64 hash = calcHash(num_frames, frames);

    LongHashTable* table = _current_table;
    u64* keys = table->keys();
    u32 capacity = table->capacity();
    u32 slot = hash & (capacity - 1);
    u32 step = 0;

    while (keys[slot] != hash) {
        if (keys[slot] == 0) {
            if (!__sync_bool_compare_and_swap(&keys[slot], 0, hash)) {
                continue;
            }

            // Increment the table size, and if the load factor exceeds 0.75, reserve a new table
            if (table->incSize() == capacity * 3 / 4) {
                LongHashTable* new_table = LongHashTable::allocate(table, capacity * 2);
                if (new_table != NULL) {
                    __sync_bool_compare_and_swap(&_current_table, table, new_table);
                }
            }

            // Migrate from a previous table to save space
            CallTrace* trace = table->prev() == NULL ? NULL : findCallTrace(table->prev(), hash);
            if (trace == NULL) {
                trace = storeCallTrace(num_frames, frames);
            }
            table->values()[slot].setTrace(trace);
            break;
        }

        if (++step >= capacity) {
            // Very unlikely case of a table overflow
            atomicInc(_overflow);
            return OVERFLOW_TRACE_ID;
        }
        // Improved version of linear probing
        slot = (slot + step) & (capacity - 1);
    }

    if (counter != 0) {
        CallTraceSample& s = table->values()[slot];
        atomicInc(s.samples);
        atomicInc(s.counter, counter);
    }

    return capacity - (INITIAL_CAPACITY - 1) + slot;
}

void CallTraceStorage::add(u32 call_trace_id, u64 samples, u64 counter) {
    if (call_trace_id == OVERFLOW_TRACE_ID) {
        return;
    }

    call_trace_id += (INITIAL_CAPACITY - 1);
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        if (call_trace_id >= table->capacity()) {
            CallTraceSample& s = table->values()[call_trace_id - table->capacity()];
            atomicInc(s.samples, samples);
            atomicInc(s.counter, counter);
            break;
        }
    }
}

void CallTraceStorage::resetCounters() {
     for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        u64* keys = table->keys();
        CallTraceSample* values = table->values();
        u32 capacity = table->capacity();

        for (u32 slot = 0; slot < capacity; slot++) {
            if (keys[slot] != 0) {
                CallTraceSample& s = values[slot];
                storeRelease(s.samples, 0);
                storeRelease(s.counter, 0);
            }
        }
    }
}
