/*
 * Copyright 2021, 2022 Datadog, Inc
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

#include "context.h"
#include "os.h"
#include "vmEntry.h"

#define BYTE_TO_BITS(by) ((by) * 8)
#define ELSIZE (BYTE_TO_BITS(sizeof(BitsetElement)))
#define ELINDEX(i) ((i) / BYTE_TO_BITS(sizeof(BitsetElement)))
#define ELOFFSET(i) ((i) % BYTE_TO_BITS(sizeof(BitsetElement)))

int Contexts::_contexts_size = -1;
Context* Contexts::_contexts = NULL;

bool Contexts::_wall_filtering = true;
bool Contexts::_cpu_filtering = true;

BitsetElement* Contexts::_threads = NULL;
int Contexts::_threads_size = 0;

Context Contexts::get(int tid) {
    return _contexts[tid];
}

bool Contexts::filter(Context ctx, int event_type) {
    switch (event_type) {
    case BCI_WALL:
        return !_wall_filtering || ctx.spanId > 0;
    case BCI_CPU:
        return !_cpu_filtering || ctx.spanId > 0;
    default:
        // no filtering based on context
        return true;
    }
}

bool Contexts::filter(int tid, int event_type) {
    // the thread should be suspended, so _contexts[tid] shouldn't change
    Context context = _contexts[tid];

    return filter(context, event_type);
}

void Contexts::set(int tid, Context context) {
    bool installed;
    installed = _contexts[tid].spanId == 0;
    _contexts[tid] = context;
    if (installed == 0) {
        // FIXME: reenable when using thread filtering based on context
        // registerThread(tid);
    }
}

void Contexts::clear(int tid) {
    // FIXME: reenable when using thread filtering based on context
    // unregisterThread(tid);
    _contexts[tid].spanId = 0;
    _contexts[tid].rootSpanId = 0;
}

void Contexts::initialize() {
    if (__atomic_load_n(&_contexts, __ATOMIC_ACQUIRE) == NULL) {
        Context *contexts = (Context*)calloc(_contexts_size = OS::getMaxThreadId(), sizeof(Context));
        if (!__sync_bool_compare_and_swap(&_contexts, NULL, contexts)) {
            free(contexts);
        }
    }
}

ContextStorage Contexts::getStorage() {
    initialize();
    return {.capacity = _contexts_size * (int) sizeof(Context), .storage = _contexts};
}

void Contexts::registerThread(int tid) {
    if (!(_wall_filtering || _cpu_filtering)) {
        // Only track threads when filtering is enabled
        return;
    }

    if (!__atomic_load_n(&_threads, __ATOMIC_ACQUIRE)) {
        BitsetElement* threads =
            (BitsetElement*)calloc(
                _threads_size = ELINDEX(OS::getMaxThreadId()),
                    sizeof(BitsetElement));
        if (!__sync_bool_compare_and_swap(&_threads, NULL, threads)) {
            free(threads);
        }
    }

    int index = ELINDEX(tid);
    int offset = ELOFFSET(tid);

    BitsetElement old_bitset, new_bitset;
    do {
        old_bitset = _threads[index];
        new_bitset = old_bitset | (1ULL << offset);
    } while (!__sync_bool_compare_and_swap(&_threads[index], old_bitset, new_bitset));
}

void Contexts::unregisterThread(int tid) {
    if (!(_wall_filtering || _cpu_filtering)) {
        // Only track threads when filtering is enabled
        return;
    }

    if (!__atomic_load_n(&_threads, __ATOMIC_ACQUIRE)) {
        return;
    }

    int index = ELINDEX(tid);
    int offset = ELOFFSET(tid);

    BitsetElement old_bitset, new_bitset;
    do {
        old_bitset = _threads[index];
        new_bitset = old_bitset & ~(1ULL << offset);
    } while (!__sync_bool_compare_and_swap(&_threads[index], old_bitset, new_bitset));
}


class ContextsThreadList : public ThreadList {
    int _last;

  public:
    ContextsThreadList() : _last(-1) {}

    virtual ~ContextsThreadList() {}

    virtual void rewind() {
        _last = -1;
    }

    virtual int next() {
        for (int index = ELINDEX(_last + 1); index < Contexts::_threads_size; index += 1) {
            BitsetElement bitset = Contexts::_threads[index];
            if (bitset != 0) {
                for (int offset = index == ELINDEX(_last) ? ELOFFSET(_last + 1) : 0; offset < ELSIZE; offset++) {
                    int tid = index * ELSIZE + offset;
                    if ((bitset & (1ULL << offset))) {
                        return (_last = tid);
                    }
                }
            }
        }

        return -1;
    }

    virtual int size() {
        int count = 0;
        for (int index = 0; index < Contexts::_threads_size; index += 1) {
            count += __builtin_popcountll(Contexts::_threads[index]);
        }
        return count;
    }
};

ThreadList* Contexts::listThreads() {
    // FIXME: study whether using Contexts::listThreads() introduces bias
    return new ContextsThreadList();
}
