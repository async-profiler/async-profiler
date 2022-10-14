/*
 * Copyright 2022 Andrei Pangin
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

#include <string.h>
#include "objectSampler.h"
#include "profiler.h"


u64 ObjectSampler::_interval;
bool ObjectSampler::_live;
volatile u64 ObjectSampler::_allocated_bytes;


class LiveRefs {
  private:
    enum { MAX_REFS = 1024 };

    SpinLock _lock;
    jweak _refs[MAX_REFS];
    jlong _sizes[MAX_REFS];
    u32 _traces[MAX_REFS];

    static inline bool collected(jweak w) {
        return *(void**)((uintptr_t)w & ~(uintptr_t)1) == NULL;
    }

    void set(u32 index, jweak w, jlong size, u32 call_trace_id) {
        _refs[index] = w;
        _sizes[index] = size;
        _traces[index] = call_trace_id;
    }

  public:
    LiveRefs() : _lock(1) {
    }

    void init() {
        memset(_refs, 0, sizeof(_refs));
        memset(_sizes, 0, sizeof(_sizes));
        memset(_traces, 0, sizeof(_traces));

        _lock.unlock();
    }

    void add(JNIEnv* jni, jobject object, jlong size, u32 call_trace_id) {
        jweak wobject = jni->NewWeakGlobalRef(object);
        if (wobject == NULL) {
            return;
        }

        if (_lock.tryLock()) {
            jlong min_size = size;
            u32 min_index = 0;

            u32 start = (((uintptr_t)object >> 4) * 31 + ((uintptr_t)jni >> 4) + call_trace_id) & (MAX_REFS - 1);
            u32 i = start;
            do {
                jweak w = _refs[i];
                if (w == NULL) {
                    set(i, wobject, size, call_trace_id);
                    _lock.unlock();
                    return;
                } else if (collected(w)) {
                    jni->DeleteWeakGlobalRef(w);
                    set(i, wobject, size, call_trace_id);
                    _lock.unlock();
                    return;
                } else if (_sizes[i] < min_size) {
                    min_size = _sizes[i];
                    min_index = i;
                }
            } while ((i = (i + 1) & (MAX_REFS - 1)) != start);

            if (min_size < size) {
                jni->DeleteWeakGlobalRef(_refs[min_index]);
                set(min_index, wobject, size, call_trace_id);
                _lock.unlock();
                return;
            }

            _lock.unlock();
        }

        jni->DeleteWeakGlobalRef(wobject);
    }

    void dump(JNIEnv* jni) {
        _lock.lock();

        for (u32 i = 0; i < MAX_REFS; i++) {
            jweak w = _refs[i];
            if (w != NULL) {
                if (!collected(w)) {
                    Profiler::instance()->callTraceStorage()->add(_traces[i], _sizes[i]);
                }
                jni->DeleteWeakGlobalRef(w);
            }
        }
    }
};

static LiveRefs live_refs;


void ObjectSampler::SampledObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                       jobject object, jclass object_klass, jlong size) {
    if (_enabled) {
        recordAllocation(jvmti, jni, BCI_ALLOC, object, object_klass, size);
    }
}

void ObjectSampler::recordAllocation(jvmtiEnv* jvmti, JNIEnv* jni, int event_type,
                                     jobject object, jclass object_klass, jlong size) {
    AllocEvent event;
    event._class_id = 0;
    event._total_size = size > _interval ? size : _interval;
    event._instance_size = size;

    char* class_name;
    if (jvmti->GetClassSignature(object_klass, &class_name, NULL) == 0) {
        if (class_name[0] == 'L') {
            event._class_id = Profiler::instance()->classMap()->lookup(class_name + 1, strlen(class_name) - 2);
        } else {
            event._class_id = Profiler::instance()->classMap()->lookup(class_name);
        }
        jvmti->Deallocate((unsigned char*)class_name);
    }

    if (_live) {
        u32 call_trace_id = Profiler::instance()->recordSample(NULL, 0, event_type, &event);
        live_refs.add(jni, object, size, call_trace_id);
    } else {
        Profiler::instance()->recordSample(NULL, size, event_type, &event);
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

Error ObjectSampler::check(Arguments& args) {
    if (!VM::canSampleObjects()) {
        return Error("SampledObjectAlloc is not supported on this JVM");
    }
    return Error::OK;
}

Error ObjectSampler::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    _interval = args._alloc > 0 ? args._alloc : DEFAULT_ALLOC_INTERVAL;

    initLiveRefs(args._live);

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetHeapSamplingInterval(_interval);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);

    return Error::OK;
}

void ObjectSampler::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);

    dumpLiveRefs();
}
