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

#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "arch.h"
#include "arguments.h"
#include "os.h"

typedef struct {
    u64 spanId;
    u64 rootSpanId;
    u64 checksum;
    u64 pad1;
    u64 pad2;
    u64 pad3;
    u64 pad4;
    u64 pad5;
} Context;

typedef struct {
    const int capacity;
    const Context* storage;
} ContextStorage;

class ContextsThreadList;

class Contexts {
    friend class ContextsThreadList;

  private:
    static int _contexts_size;
    static Context* _contexts;

  public:
    static void initialize();
    static const Context& get(int tid);
    static bool isValid(const Context& context);
    // not to be called except to share with Java callers as a DirectByteBuffer
    static ContextStorage getStorage();
};

#endif /* _CONTEXT_H */
