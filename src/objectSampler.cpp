/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <vector>
#include "objectSampler.h"
#include "profiler.h"
#include "tsc.h"

u64 ObjectSampler::_interval;
bool ObjectSampler::_live;
size_t ObjectSampler::_live_gc_threshold;
volatile u64 ObjectSampler::_allocated_bytes;
size_t ObjectSampler::_observed_gc_starts = 0;

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

struct LiveObjectInfo {
    jlong size;
    u64 trace;
    u64 time;
    size_t gc_count;
};

class LiveRefs {
  private:
    SpinLock _lock;
    std::vector<jweak> _refs;
    std::vector<LiveObjectInfo> _values;
    bool _full;

    static inline bool collected(jweak w) {
        return *(void**)((uintptr_t)w & ~(uintptr_t)1) == NULL;
    }

  public:
    LiveRefs() :
        _lock(1),
        _refs(0),
        _values(0) {
    }

    void init(size_t count) {
        _full = false;
        _refs.clear();
        _refs.resize(count);
        _values.clear();
        _values.resize(count);
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
            u32 start = (((uintptr_t)object >> 4) * 31 + ((uintptr_t)jni >> 4) + trace) & (_refs.size() - 1);
            u32 i = start;
            do {
                jweak w = _refs[i];
                if (w == NULL || collected(w)) {
                    if (w != NULL) jni->DeleteWeakGlobalRef(w);
                    _refs[i] = wobject;
                    _values[i].size = size;
                    _values[i].trace = trace;
                    _values[i].time = TSC::ticks();
                    _values[i].gc_count = ObjectSampler::current_gc_counter();
                    _lock.unlock();
                    return;
                }
            } while ((i = (i + 1) & (_refs.size() - 1)) != start);

            _full = true;
            _lock.unlock();
        }

        jni->DeleteWeakGlobalRef(wobject);
    }

    void dump(JNIEnv* jni, size_t minimum_gc_count) {
        _lock.lock();

        jvmtiEnv* jvmti = VM::jvmti();
        Profiler* profiler = Profiler::instance();

        // Reset counters before dumping to collect live objects only.
        profiler->tryResetCounters();

        size_t current_gc_counter = ObjectSampler::current_gc_counter();

        for (u32 i = 0; i < _refs.size(); i++) {
            if ((i % 32) == 0) jni->PushLocalFrame(64);

            jweak w = _refs[i];
            if (w != NULL) {
                // We can optionally filter objects based on how many GCs they
                // survive.
                //
                // Our heuristic is to compare the current observed GC count against
                // the GC count when the object was allocated. If the delta exceeds
                // our GC count threshold, we can emit the object.
                //
                // The GC counter is incremented during GarbageCollectionStart JVMTI
                // events and we only consider the GC start events. There should be
                // no race condition for events allocated between GarbageCollectionStart
                // and GarbageCollectionFinish because the VM is paused between these
                // events and no allocations should occur. Therefore the counts of GC
                // start and finish JVMTI events are effectively identical.

                bool meets_gc_filter = false;
                if (minimum_gc_count == 0) {
                    meets_gc_filter = true;
                } else if (_values[i].gc_count > current_gc_counter) {
                    // integer wraparound. Should be rare. Don't bother with
                    // complexity. Don't emit.
                    meets_gc_filter = false;
                } else {
                    meets_gc_filter = current_gc_counter - _values[i].gc_count >= minimum_gc_count;
                }

                if (meets_gc_filter) {
                    jobject obj = jni->NewLocalRef(w);
                    if (obj != NULL) {
                        LiveObject event;
                        event._start_time = TSC::ticks();
                        event._alloc_size = _values[i].size;
                        event._alloc_time = _values[i].time;
                        event._class_id = lookupClassId(jvmti, jni->GetObjectClass(obj));

                        int tid = _values[i].trace >> 32;
                        u32 call_trace_id = (u32) _values[i].trace;
                        profiler->recordExternalSamples(1, event._alloc_size, tid, call_trace_id, LIVE_OBJECT, &event);
                    }
                }
                jni->DeleteWeakGlobalRef(w);
            }

            if ((i % 32) == 31 || i == _refs.size() - 1) jni->PopLocalFrame(NULL);
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
    _observed_gc_starts++;
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

void ObjectSampler::initLiveRefs(bool live, int ringsize, int live_gc_threshold) {
    _live = live;
    _live_gc_threshold = live_gc_threshold;
    if (_live) {
        live_refs.init(ringsize);
    }
}

void ObjectSampler::dumpLiveRefs() {
    if (_live) {
        live_refs.dump(VM::jni(), _live_gc_threshold);
    }
}

Error ObjectSampler::start(Arguments& args) {
    _interval = args._alloc > 0 ? args._alloc : DEFAULT_ALLOC_INTERVAL;

    initLiveRefs(args._live, args._livebuffersize, args._live_gc_threshold);

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
