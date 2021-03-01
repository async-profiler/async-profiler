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
#include "os.h"
#include "profiler.h"
#include "vmStructs.h"


jlong LockTracer::_threshold;
jlong LockTracer::_start_time = 0;
jclass LockTracer::_LockSupport = NULL;
jmethodID LockTracer::_getBlocker = NULL;

Error LockTracer::start(Arguments& args) {
    _threshold = args._lock;

    // Enable Java Monitor events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
    _start_time = OS::nanotime();

    if (_getBlocker == NULL) {
        JNIEnv* env = VM::jni();
        _LockSupport = (jclass)env->NewGlobalRef(env->FindClass("java/util/concurrent/locks/LockSupport"));
        _getBlocker = env->GetStaticMethodID(_LockSupport, "getBlocker", "(Ljava/lang/Thread;)Ljava/lang/Object;");
    }

    // Intercept Unsafe.park() for tracing contended ReentrantLocks
    if (VMStructs::_unsafe_park != NULL) {
        bindUnsafePark(UnsafeParkTrap);
    }

    return Error::OK;
}

void LockTracer::stop() {
    // Disable Java Monitor events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);

    // Reset Unsafe.park() trap
    if (VMStructs::_unsafe_park != NULL) {
        bindUnsafePark(VMStructs::_unsafe_park);
    }
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong enter_time = OS::nanotime();
    jvmti->SetTag(thread, enter_time);
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong entered_time = OS::nanotime();
    jlong enter_time;
    jvmti->GetTag(thread, &enter_time);

    // Time is meaningless if lock attempt has started before profiling
    if (_enabled && entered_time - enter_time >= _threshold && enter_time >= _start_time) {
        char* lock_name = getLockName(jvmti, env, object);
        recordContendedLock(BCI_LOCK, enter_time, entered_time, lock_name, object, 0);
        jvmti->Deallocate((unsigned char*)lock_name);
    }
}

void JNICALL LockTracer::UnsafeParkTrap(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time) {
    jvmtiEnv* jvmti = VM::jvmti();
    jobject park_blocker = _enabled ? getParkBlocker(jvmti, env) : NULL;
    jlong park_start_time, park_end_time;

    if (park_blocker != NULL) {
        park_start_time = OS::nanotime();
    }

    VMStructs::_unsafe_park(env, instance, isAbsolute, time);

    if (park_blocker != NULL) {
        park_end_time = OS::nanotime();
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
    LockEvent event;
    event._class_id = 0;
    event._start_time = start_time;
    event._end_time = end_time;
    event._address = *(uintptr_t*)lock;
    event._timeout = timeout;

    if (lock_name != NULL) {
        if (lock_name[0] == 'L') {
            event._class_id = Profiler::_instance.classMap()->lookup(lock_name + 1, strlen(lock_name) - 2);
        } else {
            event._class_id = Profiler::_instance.classMap()->lookup(lock_name);
        }
    }

    Profiler::_instance.recordSample(NULL, end_time - start_time, event_type, &event);
}

void LockTracer::bindUnsafePark(UnsafeParkFunc entry) {
    JNIEnv* env = VM::jni();

    // Try JDK 9+ package first, then fallback to JDK 8 package
    jclass unsafe = env->FindClass("jdk/internal/misc/Unsafe");
    if (unsafe == NULL) unsafe = env->FindClass("sun/misc/Unsafe");

    if (unsafe != NULL) {
        const JNINativeMethod unsafe_park = {(char*)"park", (char*)"(ZJ)V", (void*)entry};
        env->RegisterNatives(unsafe, &unsafe_park, 1);
    }

    env->ExceptionClear();
}
