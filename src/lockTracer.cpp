/*
 * Copyright 2017 Andrei Pangin
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
#include "lockTracer.h"
#include "profiler.h"
#include "tsc.h"
#include "context.h"
#include "thread.h"

volatile bool LockTracer::_enabled = false;

double LockTracer::_ticks_to_nanos;
jlong LockTracer::_threshold;
jlong LockTracer::_start_time = 0;
jclass LockTracer::_UnsafeClass = NULL;
jclass LockTracer::_LockSupport = NULL;
jmethodID LockTracer::_getBlocker = NULL;
RegisterNativesFunc LockTracer::_orig_RegisterNatives = NULL;
UnsafeParkFunc LockTracer::_orig_Unsafe_park = NULL;
bool LockTracer::_initialized = false;

Error LockTracer::start(Arguments& args) {
    _ticks_to_nanos = 1e9 / TSC::frequency();
    _threshold = (jlong)(args._lock * (TSC::frequency() / 1e9));

    if (!_initialized) {
        initialize();
    }

    // Enable Java Monitor events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
    _start_time = TSC::ticks();

    // Intercept Unsafe.park() for tracing contended ReentrantLocks
    if (_orig_Unsafe_park != NULL) {
        bindUnsafePark(UnsafeParkHook);
    }

    return Error::OK;
}

void LockTracer::stop() {
    // Disable Java Monitor events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);

    // Reset Unsafe.park() trap
    if (_orig_Unsafe_park != NULL) {
        bindUnsafePark(_orig_Unsafe_park);
    }
}

void LockTracer::initialize() {
    jvmtiEnv* jvmti = VM::jvmti();
    JNIEnv* env = VM::jni();

    // Try JDK 9+ package first, then fallback to JDK 8 package
    jclass unsafe = env->FindClass("jdk/internal/misc/Unsafe");
    if (unsafe == NULL) {
        env->ExceptionClear();
        if ((unsafe = env->FindClass("sun/misc/Unsafe")) == NULL) {
            env->ExceptionClear();
            return;
        }
    }

    _UnsafeClass = (jclass)env->NewGlobalRef(unsafe);
    jmethodID register_natives = env->GetStaticMethodID(_UnsafeClass, "registerNatives", "()V");
    jniNativeInterface* jni_functions;
    if (register_natives != NULL && jvmti->GetJNIFunctionTable(&jni_functions) == 0) {
        _orig_RegisterNatives = jni_functions->RegisterNatives;
        jni_functions->RegisterNatives = RegisterNativesHook;
        jvmti->SetJNIFunctionTable(jni_functions);

        // Trace Unsafe.registerNatives() to find the original address of Unsafe.park() native  
        env->CallStaticVoidMethod(_UnsafeClass, register_natives);

        jni_functions->RegisterNatives = _orig_RegisterNatives;
        jvmti->SetJNIFunctionTable(jni_functions);
    }

    if (_orig_Unsafe_park == NULL) {
        // For OpenJ9
        _orig_Unsafe_park = (UnsafeParkFunc)Profiler::instance()->resolveSymbol("Java_sun_misc_Unsafe_park");
    }

    _LockSupport = (jclass)env->NewGlobalRef(env->FindClass("java/util/concurrent/locks/LockSupport"));
    _getBlocker = env->GetStaticMethodID(_LockSupport, "getBlocker", "(Ljava/lang/Thread;)Ljava/lang/Object;");

    env->ExceptionClear();
    _initialized = true;
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong enter_time = TSC::ticks();
    jvmti->SetTag(thread, enter_time);
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong entered_time = TSC::ticks();
    jlong enter_time;
    jvmti->GetTag(thread, &enter_time);

    // Time is meaningless if lock attempt has started before profiling
    if (_enabled && entered_time - enter_time >= _threshold && enter_time >= _start_time) {
        char* lock_name = getLockName(jvmti, env, object);
        recordContendedLock(BCI_LOCK, enter_time, entered_time, lock_name, object, 0);
        jvmti->Deallocate((unsigned char*)lock_name);
    }
}

jint JNICALL LockTracer::RegisterNativesHook(JNIEnv* env, jclass cls, const JNINativeMethod* methods, jint nMethods) {
    if (env->IsSameObject(cls, _UnsafeClass)) {
        for (jint i = 0; i < nMethods; i++) {
            if (strcmp(methods[i].name, "park") == 0 && strcmp(methods[i].signature, "(ZJ)V") == 0) {
                _orig_Unsafe_park = (UnsafeParkFunc)methods[i].fnPtr;
                break;
            } 
        }
        return 0;
    }
    return _orig_RegisterNatives(env, cls, methods, nMethods);
}

void JNICALL LockTracer::UnsafeParkHook(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time) {
    jvmtiEnv* jvmti = VM::jvmti();
    jobject park_blocker = _enabled ? getParkBlocker(jvmti, env) : NULL;
    jlong park_start_time, park_end_time;

    if (park_blocker != NULL) {
        park_start_time = TSC::ticks();
    }

    _orig_Unsafe_park(env, instance, isAbsolute, time);

    if (park_blocker != NULL) {
        park_end_time = TSC::ticks();
        if (park_end_time - park_start_time >= _threshold) {
            char* lock_name = getLockName(jvmti, env, park_blocker);
            if (lock_name == NULL || isConcurrentLock(lock_name)) {
                recordContendedLock(BCI_PARK, park_start_time, park_end_time, lock_name, park_blocker, time);
            }
            jvmti->Deallocate((unsigned char*)lock_name);
        }
    }
}

jobject LockTracer::getParkBlocker(jvmtiEnv* jvmti, JNIEnv* env) {
    jthread thread;
    if (jvmti->GetCurrentThread(&thread) != 0) {
        return NULL;
    }

    // Call LockSupport.getBlocker(Thread.currentThread())
    return env->CallStaticObjectMethod(_LockSupport, _getBlocker, thread);
}

char* LockTracer::getLockName(jvmtiEnv* jvmti, JNIEnv* env, jobject lock) {
    char* class_name;
    if (jvmti->GetClassSignature(env->GetObjectClass(lock), &class_name, NULL) != 0) {
        return NULL;
    }
    return class_name;
}

bool LockTracer::isConcurrentLock(const char* lock_name) {
    // Do not count synchronizers other than ReentrantLock, ReentrantReadWriteLock and Semaphore
    return strncmp(lock_name, "Ljava/util/concurrent/locks/ReentrantLock", 41) == 0 ||
           strncmp(lock_name, "Ljava/util/concurrent/locks/ReentrantReadWriteLock", 50) == 0 ||
           strncmp(lock_name, "Ljava/util/concurrent/Semaphore", 31) == 0;
}

void LockTracer::recordContendedLock(int event_type, u64 start_time, u64 end_time,
                                     const char* lock_name, jobject lock, jlong timeout) {
    int tid = ProfiledThread::currentTid();
    Context ctx = Contexts::get(tid);

    LockEvent event;
    event._start_time = start_time;
    event._end_time = end_time;
    event._address = *(uintptr_t*)lock;
    event._timeout = timeout;
    event._context = ctx;

    if (lock_name != NULL) {
        if (lock_name[0] == 'L') {
            event._id = Profiler::instance()->classMap()->lookup(lock_name + 1, strlen(lock_name) - 2);
        } else {
            event._id = Profiler::instance()->classMap()->lookup(lock_name);
        }
    }

    u64 duration_nanos = (u64)((end_time - start_time) * _ticks_to_nanos);
    Profiler::instance()->recordSample(NULL, duration_nanos, tid, event_type, &event);
}

void LockTracer::bindUnsafePark(UnsafeParkFunc entry) {
    JNIEnv* env = VM::jni();
    const JNINativeMethod park = {(char*)"park", (char*)"(ZJ)V", (void*)entry};
    if (env->RegisterNatives(_UnsafeClass, &park, 1) != 0) {
        env->ExceptionClear();
    }
}
