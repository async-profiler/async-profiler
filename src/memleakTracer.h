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

#ifndef _MEMLEAKTRACER_H
#define _MEMLEAKTRACER_H

#include <jvmti.h>
#include <pthread.h>
#include "arch.h"
#include "engine.h"
#include "spinLock.h"

#define MEMLEAK_TABLE_MAX_SIZE (64 * 1024)
#define MEMLEAK_STACKFRAMES_MAX_DEPTH (64)

typedef struct MemLeakTableEntry {
    jweak ref;
    jint ref_size;
    jvmtiFrameInfo frames[MEMLEAK_STACKFRAMES_MAX_DEPTH];
    jint frames_size;
    jint tid;
    jlong time;
    jlong age;
} MemLeakTableEntry;

class MemLeakTracer : public Engine {
  private:

    static bool _initialized;

    static SpinLock _table_lock;
    static MemLeakTableEntry *_table;
    static volatile int _table_size;

    static int _max_stack_depth;

    static jclass _Class;
    static jmethodID _Class_getName;
    static jclass _MemLeak;
    static jmethodID _MemLeak_process;
    static jclass _MemLeakEntry;
    static jmethodID _MemLeakEntry_init;

    static pthread_t _cleanup_thread;
    static pthread_mutex_t _cleanup_mutex;
    static pthread_cond_t _cleanup_cond;
    static u32 _cleanup_round;
    static bool _cleanup_run;

    static Error initialize();

    static void cleanup_table(JNIEnv* env, jobjectArray *entries, jint *nentries);

    static void flush_table(JNIEnv* env);

    static void* cleanup_thread(void *arg);

  public:
    const char* title() {
        return "Memory Leak profile";
    }

    const char* units() {
        return "bytes";
    }

    Error start(Arguments& args);
    void stop();

    static void JNICALL SampledObjectAlloc(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread, jobject object, jclass object_klass, jlong size);
    static void JNICALL GarbageCollectionFinish(jvmtiEnv *jvmti_env);
};

#endif // _MEMLEAKTRACER_H
