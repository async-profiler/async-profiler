/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NATIVELOCKTRACER_H
#define _NATIVELOCKTRACER_H

#include "engine.h"
#include "event.h"
#include "mutex.h"


class NativeLockTracer : public Engine {
    private:
        static u64 _interval;

        static Mutex _patch_lock;
        static int _patched_libs;
        static bool _initialized;
        static volatile bool _running;
        static volatile u64 _total_duration;
        static u64 _start_time;

        static void initialize();
        static void patchLibraries();

    public:
        const char* type() {
            return "native_lock_tracer";
        }

        const char* title() {
            return "Native lock profile";
        }

        const char* units() {
            return "ns";
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

        static void recordNativeLock(void* address, u64 start_time, u64 end_time);
};

#endif // _NATIVELOCKTRACER_H