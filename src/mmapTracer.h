/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
#ifndef _MMAPTRACER_H
#define _MMAPTRACER_H
#include <atomic>
#include <jvmti.h>
#include "engine.h"
#include "arch.h"
class MmapTracer : public Engine {
  public:
    static std::atomic<bool> running;
    static std::atomic<bool> transforming;
    static std::atomic<unsigned long long> lost;
    static bool transform(jvmtiEnv*, const char*, jint, const u8*, jint*, u8**);
    static void installHooks();
    static unsigned long long dropped() { return lost.load(); }
    const char* type() { return "mmap_tracer"; }
    const char* title() { return "Memory mappings"; }
    const char* units() { return "bytes"; }
    Error start(Arguments& args);
    void stop();
};
#endif
