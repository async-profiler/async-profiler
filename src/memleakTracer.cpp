/*
* Copyright 2021 Datadog, Inc
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
#include "incbin.h"
#include "memleakTracer.h"
#include "os.h"
#include "profiler.h"
#include "log.h"
#include "tsc.h"
#include "vmStructs.h"


INCBIN(MEM_LEAK_CLASS, "one/profiler/MemLeak.class")
INCBIN(MEM_LEAK_ENTRY_CLASS, "one/profiler/MemLeakEntry.class")

bool MemLeakTracer::_initialized = false;
int MemLeakTracer::_interval;

SpinLock MemLeakTracer::_table_lock;
MemLeakTableEntry* MemLeakTracer::_table;
volatile int MemLeakTracer::_table_size;
int MemLeakTracer::_table_cap;
int MemLeakTracer::_table_max_cap;

int MemLeakTracer::_max_stack_depth;

jclass MemLeakTracer::_Class;
jmethodID MemLeakTracer::_Class_getName;
jclass MemLeakTracer::_MemLeak;
jmethodID MemLeakTracer::_MemLeak_process;
jclass MemLeakTracer::_MemLeakEntry;
jmethodID MemLeakTracer::_MemLeakEntry_init;

pthread_t MemLeakTracer::_cleanup_thread;
pthread_mutex_t MemLeakTracer::_cleanup_mutex;
pthread_cond_t MemLeakTracer::_cleanup_cond;
u32 MemLeakTracer::_cleanup_round;
bool MemLeakTracer::_cleanup_run;
get_sampling_interval MemLeakTracer::_get_sampling_interval;

static int __min(int a, int b) {
    return a < b ? a : b;
}

void MemLeakTracer::cleanup_table(JNIEnv* env, jobjectArray *entries, jint *nentries) {
    if (entries) {
        *nentries = 0;
    }

    u64 start = OS::nanotime(), end;

    _table_lock.lock();

    if (entries) {
        *entries = env->NewObjectArray(_table_size, _MemLeakEntry, NULL);
    }

    if (env->PushLocalFrame(_table_size + 1) != 0) {
        Log::debug("Memory Leak profiler failed to push local frame");
        _table_lock.unlock();
        return;
    }

    u32 sz, newsz = 0;
    for (u32 i = 0; i < (sz = _table_size); i++) {
        jobject ref = env->NewLocalRef(_table[i].ref);
        if (ref != NULL) {
            // it survived one more GarbageCollectionFinish event
            _table[i].age += 1;

            _table[newsz++] = _table[i];
            if (entries) {
                env->SetObjectArrayElement(*entries, (*nentries)++,
                    env->NewObject(_MemLeakEntry, _MemLeakEntry_init, _table[i].ref, _table[i].ref_size, _table[i].age));
            }
        } else {
            env->DeleteWeakGlobalRef(_table[i].ref);
            delete[] _table[i].frames;
        }

        env->DeleteLocalRef(ref);
    }

    _table_size = newsz;

    env->PopLocalFrame(NULL);

    _table_lock.unlock();

    end = OS::nanotime();
    Log::debug("Memory Leak profiler cleanup took %.2fms (%.2fus/element)",
                1.0f * (end - start) / 1000 / 1000, 1.0f * (end - start) / 1000 / sz);
}

void MemLeakTracer::flush_table(JNIEnv *env) {
    u64 start = OS::nanotime(), end;

    _table_lock.lockShared();

    u32 sz;
    for (int i = 0; i < (sz = _table_size); i++) {
        jobject ref = env->NewLocalRef(_table[i].ref);
        if (ref != NULL) {
            MemLeakEvent event;
            event._start_time = _table[i].time;
            event._age = _table[i].age;
            event._instance_size = _table[i].ref_size;
            event._interval = _table[i].interval;

            jstring name_str = (jstring)env->CallObjectMethod(env->GetObjectClass(ref), _Class_getName);
            const char *name = env->GetStringUTFChars(name_str, NULL);
            event._class_id = name != NULL ? Profiler::instance()->classMap()->lookup(name) : 0;
            env->ReleaseStringUTFChars(name_str, name);

            Profiler::instance()->recordExternalSample(_table[i].ref_size, _table[i].tid,
                                                       _table[i].frames, _table[i].frames_size, /*truncated=*/false,
                                                       BCI_MEMLEAK, &event);
        }

        env->DeleteLocalRef(ref);
    }

    _table_lock.unlockShared();

    end = OS::nanotime();
    Log::debug("Memory Leak profiler flush took %.2fms (%.2fus/element)",
                1.0f * (end - start) / 1000 / 1000, 1.0f * (end - start) / 1000 / sz);
}

void* MemLeakTracer::cleanup_thread(void *arg) {
    VM::attachThread("Async-profiler Memory Leak cleanup");

    while (true) {
        pthread_mutex_lock(&_cleanup_mutex);
        while (_cleanup_round == 0) {
            if (!_cleanup_run) {
                goto exit;
            }
            pthread_cond_wait(&_cleanup_cond, &_cleanup_mutex);
        }
        _cleanup_round -= 1;
        pthread_mutex_unlock(&_cleanup_mutex);

        jobjectArray entries = NULL;
        jint nentries = 0;

        JNIEnv *env = VM::jni();
        cleanup_table(env, &entries, &nentries);

        env->CallStaticObjectMethod(_MemLeak, _MemLeak_process, entries, nentries);
        env->ExceptionDescribe();
        env->ExceptionClear();

        env->DeleteLocalRef(entries);
    }

exit:
    VM::detachThread();
    return NULL;
}

Error MemLeakTracer::start(Arguments& args) {
    if (!_initialized) {
        Error err = initialize(args);
        if (err) { return err; }
    }

    if (VM::hotspot_version() < 11) {
        return Error("Memory Leak profiler requires Java 11+");
    }


    // Enable Java Object Sample events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);

    _interval = args._memleak;
    if (jvmti->SetHeapSamplingInterval(_interval) != JVMTI_ERROR_NONE) {
        Log::warn("Failed to set Memory Leak heap sampling interval to %d", _interval);
    }

    return Error::OK;
}

void MemLeakTracer::stop() {
    JNIEnv* env = VM::jni();
    cleanup_table(env, NULL, NULL);
    flush_table(env);

    // Disable Java Object Sample events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);
}

static int _min(int a, int b) { return a < b ? a : b; }

Error MemLeakTracer::initialize(Arguments& args) {
    // jvmtiEnv* jvmti = VM::jvmti();
    JNIEnv* env = VM::jni();

    _table_size = 0;
    _table_cap = __min(2048, args._memleak_cap); // with default 512k sampling interval, it's enough for 1G of heap
    _table_max_cap = args._memleak_cap;
    _table = (MemLeakTableEntry*)malloc(sizeof(MemLeakTableEntry) * _table_cap);

    _max_stack_depth = Profiler::instance()->max_stack_depth();

    if (!(_Class = env->FindClass("java/lang/Class"))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Unable to find java/lang/Class");
    }
    if (!(_Class_getName = env->GetMethodID(_Class, "getName", "()Ljava/lang/String;"))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Unable to find java/lang/Class.getName");
    }

    if (!(_MemLeak = env->DefineClass(NULL, NULL, (const jbyte*)MEM_LEAK_CLASS, INCBIN_SIZEOF(MEM_LEAK_CLASS))) ||
        !(_MemLeak = (jclass)env->NewGlobalRef(_MemLeak))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Failed to initialize one/profiler/MemLeak class");
    }
    if (!(_MemLeak_process = env->GetStaticMethodID(_MemLeak, "process", "([Lone/profiler/MemLeakEntry;I)V"))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Unable to find one/profiler/MemLeak.process");
    }
    if (!(_MemLeakEntry = env->DefineClass(NULL, NULL, (const jbyte*)MEM_LEAK_ENTRY_CLASS, INCBIN_SIZEOF(MEM_LEAK_ENTRY_CLASS))) ||
        !(_MemLeakEntry = (jclass)env->NewGlobalRef(_MemLeakEntry))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Failed to initialize one/profiler/MemLeakEntry class");
    }
    if (!(_MemLeakEntry_init = env->GetMethodID(_MemLeakEntry, "<init>", "(Ljava/lang/Object;IJ)V"))) {
        free(_table);
        env->ExceptionDescribe();
        return Error("Unable to find one/profiler/MemLeakEntry.<init>");
    }

    _cleanup_round = 0;
    _cleanup_run = true;
    if (pthread_mutex_init(&_cleanup_mutex, NULL) != 0 ||
        pthread_cond_init(&_cleanup_cond, NULL) != 0 ||
        pthread_create(&_cleanup_thread, NULL, cleanup_thread, NULL) != 0) {
        _cleanup_run = false;
        free(_table);
        return Error("Unable to create Memory Leak cleanup thread");
    }

    CodeCache* libjvm = VMStructs::libjvm();

    // this symbol should be availabel given the current JVTMI heap sampler implementation
    // Note: when/if that implementation would change in the future the alernatives should be added here
    const void* get_interval_ptr = libjvm->findSymbol("_ZN17ThreadHeapSampler21get_sampling_intervalEv");
    _get_sampling_interval = (get_sampling_interval)get_interval_ptr;

    if (_get_sampling_interval == NULL) {
        // fail if it is not possible to resolve the required symbol
        return Error("Unable to resolve the sampling interval getter");
    }

    env->ExceptionClear();
    _initialized = true;

    return Error::OK;
}

void JNICALL MemLeakTracer::SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv* env,
        jthread thread, jobject object, jclass object_klass, jlong size) {
    if (_table_max_cap == 0) {
        // we are not to store any objects
        return;
    }

    jweak ref = env->NewWeakGlobalRef(object);
    if (ref == NULL) {
        return;
    }

    jvmtiFrameInfo *frames = new jvmtiFrameInfo[_max_stack_depth];
    jint frames_size = 0;
    if (jvmti->GetStackTrace(thread, 0, _max_stack_depth,
                                frames, &frames_size) != JVMTI_ERROR_NONE || frames_size <= 0) {
        delete[] frames;
        return;
    }

    bool retried = false;

retry:
    if (!_table_lock.tryLockShared()) {
        // another thread is holding the non-shared lock
        delete[] frames;
        return;
    }

    // Increment _table_size in a thread-safe manner (CAS) and store the new value in idx
    // It bails out if _table_size would overflow _table_cap
    int idx;
    do {
        idx = _table_size;
    } while (idx < _table_cap &&
                !__sync_bool_compare_and_swap(&_table_size, idx, idx + 1));

    if (idx < _table_cap) {
        _table[idx].ref = ref;
        _table[idx].ref_size = size;
        _table[idx].interval = _get_sampling_interval();
        _table[idx].age = 0;
        _table[idx].frames_size = frames_size;
        _table[idx].frames = new jvmtiFrameInfo[_table[idx].frames_size];
        memcpy(_table[idx].frames, frames, sizeof(jvmtiFrameInfo) * _table[idx].frames_size);
        _table[idx].tid = OS::threadId();
        _table[idx].time = TSC::ticks();
    }

    _table_lock.unlockShared();

    if (idx == _table_cap) {
        if (!retried && _table_cap < _table_max_cap) {
            // guarantees we don't busy loop until memory exhaustion
            retried = true;

            // Let's increase the size of the table
            // This should only ever happen when sampling interval * size of table
            // is smaller than maximum heap size. So we only support increasing
            // the size of the table, not decreasing it.
            _table_lock.lock();

            // Only increase the size of the table to 8k elements
            int newcap = __min(_table_cap * 2, _table_max_cap);
            if (_table_cap != newcap) {
                _table = (MemLeakTableEntry*)realloc(_table, sizeof(MemLeakTableEntry) * (_table_cap = newcap));
                Log::debug("Increased size of Memory Leak table to %d entries", _table_cap);
            }

            _table_lock.unlock();

            goto retry;
        } else {
            Log::debug("Cannot add sampled object to Memory Leak table, it's overflowing");
        }
    }

    delete[] frames;
}

void JNICALL MemLeakTracer::GarbageCollectionFinish(jvmtiEnv *jvmti_env) {
    if (!_initialized) {
        return;
    }

    if (pthread_mutex_lock(&_cleanup_mutex) != 0) {
        Log::debug("Unable to lock Memory Leak cleanup mutex in GarbageCollectionFinish");
        return;
    }
    _cleanup_round += 1;
    pthread_cond_signal(&_cleanup_cond);
    pthread_mutex_unlock(&_cleanup_mutex);
}
