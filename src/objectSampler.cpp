/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "objectSampler.h"
#include "profiler.h"
#include "tsc.h"


u64 ObjectSampler::_interval;
bool ObjectSampler::_live;
volatile u64 ObjectSampler::_allocated_bytes;


static u32 lookupClassId(jvmtiEnv* jvmti, jclass cls) {
    u32 class_id = 0;
    char* class_name;
    if (jvmti->GetClassSignature(cls, &class_name, NULL) == 0) {
        if (class_name[0] == 'L') {
            class_id = Profiler::instance()->classMap()->lookup(class_name + 1, strlen(class_name) - 2);
        } else {
            class_id = Profiler::instance()->classMap()->lookup(class_name);
        }
        jvmti->Deallocate((unsigned char*)class_name);
    }
    return class_id;
}


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
                    event._class_id = lookupClassId(jvmti, jni->GetObjectClass(obj));

                    int tid = _values[i].trace >> 32;
                    u32 call_trace_id = (u32)_values[i].trace;
                    profiler->recordExternalSamples(1, event._alloc_size, tid, call_trace_id, LIVE_OBJECT, &event);
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
    event._class_id = lookupClassId(jvmti, object_klass);

    u64 trace = Profiler::instance()->recordSample(NULL, event._total_size, event_type, &event);
    if (_live && trace != 0) {
        live_refs.add(jni, object, size, trace);
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
