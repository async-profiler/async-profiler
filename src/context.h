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
    u64 spanId;
    u64 rootSpanId;
} ContextSnapshot;

// must be kept in sync with PAGE_SIZE in AsyncProfiler.java
const u32 PAGE_SIZE = 1024;

typedef struct {
    const int capacity;
    const Context* storage;
} ContextPage;

class Contexts {

  private:
    static Context** _pages;
    static void initialize(int pageIndex);

  public:
    // get must not allocate
    static const ContextSnapshot get(int tid);
    // not to be called except to share with Java callers as a DirectByteBuffer
    static ContextPage getPage(int tid);
    static int getMaxPages();
};

#endif /* _CONTEXT_H */
