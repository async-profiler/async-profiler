/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <string.h>
#include "lockTracer.h"
#include "profiler.h"
#include "tsc.h"

double LockTracer::_ticks_to_nanos;
jlong LockTracer::_interval;
volatile u64 LockTracer::_total_duration = 0; // for interval sampling.
jlong LockTracer::_start_time = 0;
jclass LockTracer::_UnsafeClass = NULL;
jclass LockTracer::_LockSupport = NULL;
jmethodID LockTracer::_getBlocker = NULL;
RegisterNativesFunc LockTracer::_orig_RegisterNatives = NULL;
UnsafeParkFunc LockTracer::_orig_Unsafe_park = NULL;
bool LockTracer::_initialized = false;

static pthread_key_t lock_tracer_tls = (pthread_key_t)0;

Error LockTracer::start(Arguments& args) {
    _ticks_to_nanos = 1e9 / TSC::frequency();
    _interval = (jlong)(args._lock * (TSC::frequency() / 1e9));

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
    // On 64-bit platforms, we can store lock time in a pthread local
    if (sizeof(void*) >= sizeof(jlong)) {
        pthread_key_create(&lock_tracer_tls, NULL);
    }

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

    _LockSupport = (jclass)env->NewGlobalRef(env->FindClass("java/util/concurrent/locks/LockSupport"));
    _getBlocker = env->GetStaticMethodID(_LockSupport, "getBlocker", "(Ljava/lang/Thread;)Ljava/lang/Object;");

    env->ExceptionClear();
    _initialized = true;
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong enter_time = TSC::ticks();
    if (sizeof(void*) >= sizeof(jlong) && lock_tracer_tls) {
        pthread_setspecific(lock_tracer_tls, (void*)enter_time);
    } else {
        jvmti->SetTag(thread, enter_time);
    }
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    const jlong entered_time = TSC::ticks();
    jlong enter_time;
    if (sizeof(void*) >= sizeof(jlong) && lock_tracer_tls) {
        enter_time = (jlong)pthread_getspecific(lock_tracer_tls);
    } else {
        jvmti->GetTag(thread, &enter_time);
    }

    // Time is meaningless if lock attempt has started before profiling
    const jlong duration = entered_time - enter_time;

    // When the duration accumulator overflows _interval, the event is sampled.
    if (_enabled && enter_time >= _start_time && updateCounter(_total_duration, duration, _interval)) {
        char* lock_name = getLockName(jvmti, env, object);
        recordContendedLock(LOCK_SAMPLE, enter_time, entered_time, lock_name, object, 0);
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
        const jlong duration = park_end_time - park_start_time;
        if (updateCounter(_total_duration, duration, _interval)) {
            char* lock_name = getLockName(jvmti, env, park_blocker);
            if (lock_name == NULL || isConcurrentLock(lock_name)) {
                recordContendedLock(PARK_SAMPLE, park_start_time, park_end_time, lock_name, park_blocker, time);
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

void LockTracer::recordContendedLock(EventType event_type, u64 start_time, u64 end_time,
                                     const char* lock_name, jobject lock, jlong timeout) {
    LockEvent event;
    event._class_id = 0;
    event._start_time = start_time;
    event._end_time = end_time;
    event._address = *(uintptr_t*)lock;
    event._timeout = timeout;

    if (lock_name != NULL) {
        if (lock_name[0] == 'L') {
            event._class_id = Profiler::instance()->classMap()->lookup(lock_name + 1, strlen(lock_name) - 2);
        } else {
            event._class_id = Profiler::instance()->classMap()->lookup(lock_name);
        }
    }

    u64 duration_nanos = (u64)((end_time - start_time) * _ticks_to_nanos);
    Profiler::instance()->recordSample(NULL, duration_nanos, event_type, &event);
}

void LockTracer::bindUnsafePark(UnsafeParkFunc entry) {
    JNIEnv* env = VM::jni();
    const JNINativeMethod park = {(char*)"park", (char*)"(ZJ)V", (void*)entry};
    if (env->RegisterNatives(_UnsafeClass, &park, 1) != 0) {
        env->ExceptionClear();
    }
}
