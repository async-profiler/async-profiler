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
    static volatile bool _running;

    static void initialize();
    static void patchLibraries();

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

    Error start(Arguments& args);
    void stop();

    static inline bool running() {
        return _running;
    }

    static inline void installHooks() {
        if (running()) {
            patchLibraries();
        }
    }

    static void recordMalloc(void* address, size_t size);
    static void recordFree(void* address);
};

#endif // _MALLOCTRACER_H
