/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <limits.h>
#include <string.h>
#include "objectSampler.h"
#include "profiler.h"
#include "tsc.h"


u64 ObjectSampler::_base_interval;
volatile u64 ObjectSampler::_interval;
u64 ObjectSampler::_alloc_samples;
bool ObjectSampler::_live;
volatile u64 ObjectSampler::_allocated_bytes;
volatile u64 ObjectSampler::_window_start;
volatile u64 ObjectSampler::_window_samples;

const u64 ALLOC_WINDOW_SEC = 30;


class LiveRefs {
  private:
    enum { MAX_REFS = 1024 };

    SpinLock _lock;
    jweak _refs[MAX_REFS];
    struct {
        jlong size;
        u64 trace;
        u64 time;
    } _values[MAX_REFS];
    bool _full;

    static inline bool collected(jweak w) {
        return *(void**)((uintptr_t)w & ~(uintptr_t)1) == NULL;
    }

  public:
    LiveRefs() : _lock(1) {
    }

    void init() {
        memset(_refs, 0, sizeof(_refs));
        memset(_values, 0, sizeof(_values));
        _full = false;
        _lock.unlock();
    }

    void gc() {
        _full = false;
    }

    void add(JNIEnv* jni, jobject object, jlong size, u64 trace) {
        if (_full) {
            return;
        }

        jweak wobject = jni->NewWeakGlobalRef(object);
        if (wobject == NULL) {
            return;
        }

        if (_lock.tryLock()) {
            u32 start = (((uintptr_t)object >> 4) * 31 + ((uintptr_t)jni >> 4) + trace) & (MAX_REFS - 1);
            u32 i = start;
            do {
                jweak w = _refs[i];
                if (w == NULL || collected(w)) {
                    if (w != NULL) jni->DeleteWeakGlobalRef(w);
                    _refs[i] = wobject;
                    _values[i].size = size;
                    _values[i].trace = trace;
                    _values[i].time = TSC::ticks();
                    _lock.unlock();
                    return;
                }
            } while ((i = (i + 1) & (MAX_REFS - 1)) != start);

            _full = true;
            _lock.unlock();
        }

        jni->DeleteWeakGlobalRef(wobject);
    }

    void dump(JNIEnv* jni) {
        _lock.lock();

        jvmtiEnv* jvmti = VM::jvmti();
        Profiler* profiler = Profiler::instance();

        // Reset counters before dumping to collect live objects only.
        profiler->tryResetCounters();

        for (u32 i = 0; i < MAX_REFS; i++) {
            if ((i % 32) == 0) jni->PushLocalFrame(64);

            jweak w = _refs[i];
            if (w != NULL) {
                jobject obj = jni->NewLocalRef(w);
                if (obj != NULL) {
                    LiveObject event;
                    event._start_time = TSC::ticks();
                    event._alloc_size = _values[i].size;
                    event._alloc_time = _values[i].time;

                    char* class_name = nullptr;
                    if (jvmti->GetClassSignature(jni->GetObjectClass(obj), &class_name, nullptr) == 0) {
                        event.setClassSignature(class_name);
                    }

                    int tid = _values[i].trace >> 32;
                    u32 call_trace_id = (u32)_values[i].trace;
                    profiler->recordExternalSamples(1, event._alloc_size, tid, call_trace_id, LIVE_OBJECT, &event);
                    jvmti->Deallocate((unsigned char*)class_name);
                }
                jni->DeleteWeakGlobalRef(w);
            }

            if ((i % 32) == 31 || i == MAX_REFS - 1) jni->PopLocalFrame(NULL);
        }
    }
};

static LiveRefs live_refs;


void ObjectSampler::SampledObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                       jobject object, jclass object_klass, jlong size) {
    if (_enabled) {
        recordAllocation(jvmti, jni, ALLOC_SAMPLE, object, object_klass, size);
        if (_alloc_samples > 0) {
            updateSamplingInterval();
        }
    }
}

void ObjectSampler::GarbageCollectionStart(jvmtiEnv* jvmti) {
    live_refs.gc();
}

void ObjectSampler::recordAllocation(jvmtiEnv* jvmti, JNIEnv* jni, EventType event_type,
                                     jobject object, jclass object_klass, jlong size) {
    AllocEvent event;
    event._start_time = TSC::ticks();
    event._total_size = size > _interval ? size : _interval;
    event._instance_size = size;

    char* class_name = nullptr;
    if (jvmti->GetClassSignature(object_klass, &class_name, nullptr) == 0) {
        event.setClassSignature(class_name);
    }

    u64 trace = Profiler::instance()->recordSample(NULL, event._total_size, event_type, &event);
    jvmti->Deallocate((unsigned char*)class_name);

    if (_live && trace != 0 && object != NULL) {
        live_refs.add(jni, object, size, trace);
    }
}

void ObjectSampler::updateSamplingInterval() {
    u64 window_start = _window_start;
    u64 samples = atomicInc(_window_samples) + 1;
    u64 now = TSC::ticks();
    u64 elapsed = now - window_start;
    if (samples == _alloc_samples * ALLOC_WINDOW_SEC || elapsed >= TSC::frequency() * ALLOC_WINDOW_SEC) {
        _window_start = now;
        _window_samples = 0;

        u64 rate = samples * TSC::frequency() / elapsed;
        _interval = std::min(std::max(_interval * rate / _alloc_samples, _base_interval), (u64)INT_MAX);
        VM::jvmti()->SetHeapSamplingInterval(_interval);
    }
}

void ObjectSampler::initLiveRefs(bool live) {
    _live = live;
    if (_live) {
        live_refs.init();
    }
}

void ObjectSampler::dumpLiveRefs() {
    if (_live) {
        live_refs.dump(VM::jni());
    }
}

Error ObjectSampler::start(Arguments& args) {
    _interval = args._alloc > 0 ? args._alloc : DEFAULT_ALLOC_INTERVAL;
    _base_interval = _interval;
    _alloc_samples = args._alloc_samples;
    _window_start = TSC::ticks();
    _window_samples = 0;

    initLiveRefs(args._live);

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetHeapSamplingInterval(_interval);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);

    return Error::OK;
}

void ObjectSampler::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_GARBAGE_COLLECTION_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);

    VM::releaseSampleObjectsCapability();

    dumpLiveRefs();
}
