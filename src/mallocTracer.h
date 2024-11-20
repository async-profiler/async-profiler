/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MALLOCTRACER_H
#define _MALLOCTRACER_H

#include <stdint.h>

#include "engine.h"
#include "event.h"
#include "mutex.h"
#include "trap.h"

class MallocTracer : public Engine {
  private:
    static u64 _interval;
    static volatile u64 _allocated_bytes;

    static Mutex _patch_lock;
    static int _patched_libs;
    static bool _initialized;

    static bool initialize();
    static bool patchLibs(bool install);

  public:
    const char* type() {
        return "malloc_tracer";
    }

    const char* title() {
        return "Malloc/free profile";
    }

    const char* units() {
        return "bytes";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    inline static bool installHooks() {
        return patchLibs(true);
    }

    inline static bool initialized() {
        return _initialized;
    }

    static void recordMalloc(void* address, size_t size, u64 time);
    static void recordFree(void* address, u64 time);
};

#endif // _MALLOCTRACER_H
