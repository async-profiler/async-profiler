/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OBJECTSAMPLER_H
#define _OBJECTSAMPLER_H

#include <jvmti.h>
#include "arch.h"
#include "engine.h"
#include "event.h"


class ObjectSampler : public Engine {
  protected:
    static u64 _interval;
    static bool _live;
    static size_t _live_gc_threshold;
    static volatile u64 _allocated_bytes;

    // Doesn't need to be volatile since only mutated during JVMTI callback when
    // VM is stopped.
    static size_t _observed_gc_starts;
    static void initLiveRefs(bool live, int ringsize, int live_gc_threshold);
    static void dumpLiveRefs();

    static void recordAllocation(jvmtiEnv* jvmti, JNIEnv* jni, EventType event_type,
                                 jobject object, jclass object_klass, jlong size);

  public:
    const char* type() {
        return "object_sampler";
    }

    const char* title() {
        return "Allocation profile";
    }

    const char* units() {
        return "bytes";
    }

    Error start(Arguments& args);
    void stop();

    static inline size_t current_gc_counter() {
        return _observed_gc_starts;
    }

    static void JNICALL SampledObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                           jobject object, jclass object_klass, jlong size);

    static void JNICALL GarbageCollectionStart(jvmtiEnv* jvmti);
};

#endif // _OBJECTSAMPLER_H
