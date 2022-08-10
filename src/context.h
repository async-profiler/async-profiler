/*
 * Copyright 2021 Datadog, Inc
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

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "arch.h"
#include "arguments.h"
#include "os.h"

typedef u64 BitsetElement;

typedef struct {
    volatile u64 invalid;
    volatile u64 spanId;
    volatile u64 rootSpanId;
} Context;

class ContextsThreadList;

class Contexts {
    friend class ContextsThreadList;

  private:
    static int _contexts_size;
    static Context* _contexts;
    static bool _filtering;

    static BitsetElement* _threads;
    static int _threads_size;

    static void lock(int tid);
    static void unlock(int tid);

    static void registerThread(int tid);
    static void unregisterThread(int tid);

  public:
    static void initialize();

    static void set(int tid, Context context);
    static void clear(int tid);
    static Context get(int tid);
    static bool filter(int tid, int event_type);

    static void setFiltering(bool filtering) {
        _filtering = filtering;
    }

    static ThreadList* listThreads();
};

#endif /* _CONTEXT_H */