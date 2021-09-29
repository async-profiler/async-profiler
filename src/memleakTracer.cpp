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
#include "memleakTracer.h"
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"

bool MemLeakTracer::_initialized = false;

SpinLock MemLeakTracer::_table_lock;
MemLeakTableEntry* MemLeakTracer::_table;
volatile int MemLeakTracer::_table_size;

int MemLeakTracer::_max_stack_depth;

jclass MemLeakTracer::_Class;
jmethodID MemLeakTracer::_Class_getName;

void MemLeakTracer::cleanup_entry(JNIEnv* env, MemLeakTableEntry *entry) {
    env->DeleteWeakGlobalRef(entry->ref);
}

void MemLeakTracer::cleanup_table(JNIEnv* env) {
    _table_lock.lock();

    int sz = 0;
    for (int i = 0; i < _table_size; i++) {
        jobject ref = env->NewLocalRef(_table[i].ref);
        if (ref != NULL) {
            memcpy(&_table[sz++], &_table[i], sizeof(_table[sz++]));
        } else {
            cleanup_entry(env, &_table[i]);
        }

        env->DeleteLocalRef(ref);
    }

    _table_size = sz;

    _table_lock.unlock();
}

Error MemLeakTracer::start(Arguments& args) {
    if (!_initialized) {
        initialize();
    }

    if (VM::hotspot_version() < 11) {
        return Error("Memory Leak profiler requires Java 11+");
    }

    // Enable Java Object Sample events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);

    return Error::OK;
}

void MemLeakTracer::stop() {
    JNIEnv* env = VM::jni();

    cleanup_table(env);

    _table_lock.lockShared();

    for (int i = 0; i < _table_size; i++) {
        jobject ref = env->NewLocalRef(_table[i].ref);
        if (ref != NULL) {
            MemLeakEvent event;
            event._class_id = 0;
            event._start_time = _table[i].time;
            event._instance_size = _table[i].ref_size;
            if (VMStructs::hasClassNames()) {
                jstring name_str = (jstring)env->CallObjectMethod(env->GetObjectClass(ref), _Class_getName);
                const char *name = env->GetStringUTFChars(name_str, NULL);
                event._class_id = name != NULL ? Profiler::instance()->classMap()->lookup(name) : 0;
                env->ReleaseStringUTFChars(name_str, name);
            }

            Profiler::instance()->recordSample(_table[i].frames, _table[i].frames_size, _table[i].tid,
                                                _table[i].ref_size, BCI_MEMLEAK, &event);
        }

        env->DeleteLocalRef(ref);
    }

    _table_lock.unlockShared();

    // Disable Java Object Sample events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_SAMPLED_OBJECT_ALLOC, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_GARBAGE_COLLECTION_FINISH, NULL);
}

static int _min(int a, int b) { return a < b ? a : b; }

void MemLeakTracer::initialize() {
    // jvmtiEnv* jvmti = VM::jvmti();
    JNIEnv* env = VM::jni();

    _table = new MemLeakTableEntry[MEMLEAK_TABLE_MAX_SIZE];
    _table_size = 0;

    _max_stack_depth = _min(MEMLEAK_STACKFRAMES_MAX_DEPTH, Profiler::instance()->max_stack_depth());

    _Class = env->FindClass("java/lang/Class");
    _Class_getName = env->GetMethodID(_Class, "getName", "()Ljava/lang/String;");

    env->ExceptionClear();
    _initialized = true;
}

void JNICALL MemLeakTracer::SampledObjectAlloc(jvmtiEnv *jvmti, JNIEnv* env,
        jthread thread, jobject object, jclass object_klass, jlong size) {
    jweak ref = env->NewWeakGlobalRef(object);
    if (ref == NULL) {
        return;
    }

    jvmtiFrameInfo frames[MEMLEAK_STACKFRAMES_MAX_DEPTH];
    jint frames_size;
    if (jvmti->GetStackTrace(thread, 0, _max_stack_depth,
                                frames, &frames_size) != JVMTI_ERROR_NONE || frames_size <= 0) {
        return;
    }

    _table_lock.lockShared();

    int idx = __sync_fetch_and_add(&_table_size, 1);
    _table[idx].ref = ref;
    _table[idx].ref_size = size;
    memcpy(&_table[idx].frames, &frames, sizeof(_table[idx].frames));
    _table[idx].frames_size = frames_size;
    _table[idx].tid = OS::threadId();

    _table_lock.unlockShared();
}

void JNICALL MemLeakTracer::GarbageCollectionFinish(jvmtiEnv *jvmti_env) {
    //TODO: Trigger cleanup_table on a different thread
    // Maybe, it's not a good idea as it would block MemLeakTracer::SampledObjectAlloc
    //  through acquiring _table_lock. I need to measure it.
}
