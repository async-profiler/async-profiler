/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "callTraceStorage.h"
#include "os.h"
#include <fcntl.h>


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

void CallTraceStorage::add(u32 call_trace_id, u64 counter) {
    if (call_trace_id == OVERFLOW_TRACE_ID) {
        return;
    }

    call_trace_id += (INITIAL_CAPACITY - 1);
    for (LongHashTable* table = _current_table; table != NULL; table = table->prev()) {
        if (call_trace_id >= table->capacity()) {
            CallTraceSample& s = table->values()[call_trace_id - table->capacity()];
            atomicInc(s.samples);
            atomicInc(s.counter, counter);
            break;
        }
    }
}

CallTraceStreamer::~CallTraceStreamer() {
    _attached = false;
    if (_fifo_fd >= 0)
        close(_fifo_fd);
    if (_read_fd >= 0)
        close(_read_fd);
    if (_msg_buffer) {
        for (int i = 0; i < _concurrency_level; i++) {
            if (_msg_buffer[i])
                delete[] _msg_buffer[i];
        }
        delete[] _msg_buffer;
    }
}

const char* CallTraceStreamer::attachFrameName(const char* fifo_name, int max_stack_depth, int concurrency_level, Arguments& args, int style, int epoch, Mutex& thread_names_lock, ThreadMap& thread_names) {

    _fifo_fd = open(fifo_name, O_WRONLY | O_NONBLOCK);
    if (_fifo_fd < 0) {
        if (errno == ENXIO)
            return "Fifo is not ready for reading";
        std::string err_msg = "Failed to open fifo for writing ";
        const char* err_desc = strerror(errno);
        err_msg += err_desc;
        return err_msg.c_str();
    }
    // we cannot afford SIGPIPE if collector dies
    _read_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
    if (_read_fd < 0) {
        return "Failed to open fifo for reading";
    }

    int pages = max_stack_depth * 100 / 65536;
    if (max_stack_depth * 100 % 65536 != 0)
        pages++;
    
    int estimated_pipe_buffer = pages * 65536;
    
    // attempth to increase buffer
    fcntl(_fifo_fd, F_SETPIPE_SZ, estimated_pipe_buffer);

    _buffer_size = fcntl(_fifo_fd, F_GETPIPE_SZ);
    _concurrency_level = concurrency_level;
    //if (_msg_buffer == NULL)
    _msg_buffer = new char*[concurrency_level];
    for (int i = 0; i < concurrency_level; i++) {
       //if (_msg_buffer[i])
       //     delete[] _msg_buffer[i];
        _msg_buffer[i] = new char[_buffer_size];
    }

    // 128 + 4 + _max_stack_depth = _max_stack_depth + MAX_NATIVE_FRAMES + RESERVED_FRAMES
    _fn = std::make_unique<FrameName>(args, args._style | STYLE_NO_SEMICOLON, epoch, thread_names_lock, thread_names);

    _attached = true;
    return NULL;
}

bool CallTraceStreamer::attached() const {
    return _attached;
}

int CallTraceStreamer::streamTrace(int num_frames, ASGCT_CallFrame* frames, u64 counter, int lock_index) {
    if (!_attached)
        return 0;
    // we should check overflow everywhere, but no point fo PoC
    int curr = HEADER_OFFSET;
    for (int i = 0; i < num_frames; i++) {
        const char* name = _fn->name(frames[i]);
        size_t len = strlen(name);
        for (size_t l = 0; l < len; l++, curr++) {
            _msg_buffer[lock_index][curr] = name[l];
        }
        _msg_buffer[lock_index][curr++] = ';';
    }
    _msg_buffer[lock_index][curr++] = '\n'; //debug mostly
    int body_size = curr - HEADER_OFFSET;
    _msg_buffer[lock_index][0] = body_size & 0xff;
    _msg_buffer[lock_index][1] = (body_size>>8)  & 0xff;
    _msg_buffer[lock_index][2] = (body_size>>16) & 0xff;
    _msg_buffer[lock_index][3] = (body_size>>24) & 0xff;

    int tries = 0;
    ssize_t written = 0;
    do {
        written = write(_fifo_fd, _msg_buffer[lock_index], (size_t)curr);
        tries++;
    } while (written != (ssize_t)curr && tries < TRIES_LIMIT && is_eagain());
    
    return written;
}
