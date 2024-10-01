/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <string.h>
#include "lockTracer.h"
#include "incbin.h"
#include "profiler.h"
#include "tsc.h"


// On 64-bit platforms, we can store lock time in a pthread local.
// This is faster than JVM TI SetTag/GetTag.
#define CAN_USE_TLS (sizeof(void*) >= sizeof(u64))

static pthread_key_t lock_tracer_tls = (pthread_key_t)0;

INCLUDE_HELPER_CLASS(LOCK_TRACER_NAME, LOCK_TRACER_CLASS, "one/profiler/LockTracer")


bool LockTracer::_initialized = false;
double LockTracer::_ticks_to_nanos;
u64 LockTracer::_interval;
volatile u64 LockTracer::_total_duration;  // for interval sampling
u64 LockTracer::_start_time = 0;

jclass LockTracer::_Unsafe = NULL;
jclass LockTracer::_LockTracer = NULL;
jfieldID LockTracer::_parkBlocker = NULL;
jmethodID LockTracer::_setEntry = NULL;

RegisterNativesFunc LockTracer::_orig_register_natives = NULL;
UnsafeParkFunc LockTracer::_orig_unsafe_park = NULL;


Error LockTracer::start(Arguments& args) {
    _ticks_to_nanos = 1e9 / TSC::frequency();
    _interval = (u64)(args._lock * (TSC::frequency() / 1e9));
    _total_duration = 0;

    jvmtiEnv* jvmti = VM::jvmti();
    JNIEnv* env = VM::jni();

    if (!_initialized) {
        Error error = initialize(jvmti, env);
        if (error) {
            Log::warn("ReentrantLock tracing unavailable: %s", error.message());
            env->ExceptionClear();
        }
        _initialized = true;
    }

    // Enable Java Monitor events
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
    _start_time = TSC::ticks();

    // Intercept Unsafe.park() for tracing contended ReentrantLocks
    setUnsafeParkEntry(env, UnsafeParkHook);

    return Error::OK;
}

void LockTracer::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    JNIEnv* env = VM::jni();

    // Disable Java Monitor events
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);

    // Reset Unsafe.park() trap
    setUnsafeParkEntry(env, _orig_unsafe_park);
}

Error LockTracer::initialize(jvmtiEnv* jvmti, JNIEnv* env) {
    if (CAN_USE_TLS) {
        pthread_key_create(&lock_tracer_tls, NULL);
    }

    // Try JDK 9+ package first, then fallback to JDK 8 package
    jclass unsafe = env->FindClass("jdk/internal/misc/Unsafe");
    if (unsafe == NULL) {
        env->ExceptionClear();
        if ((unsafe = env->FindClass("sun/misc/Unsafe")) == NULL) {
            return Error("Unsafe class not found");
        }
    }
    _Unsafe = (jclass)env->NewGlobalRef(unsafe);

    jmethodID register_natives = env->GetStaticMethodID(_Unsafe, "registerNatives", "()V");
    if (register_natives == NULL) {
        return Error("registerNatives method not found");
    }

    jniNativeInterface* jni_functions;
    if (jvmti->GetJNIFunctionTable(&jni_functions) == 0) {
        _orig_register_natives = jni_functions->RegisterNatives;
        jni_functions->RegisterNatives = RegisterNativesHook;
        jvmti->SetJNIFunctionTable(jni_functions);

        // Trace Unsafe.registerNatives() to find the original address of Unsafe.park() native
        env->CallStaticVoidMethod(_Unsafe, register_natives);

        jni_functions->RegisterNatives = _orig_register_natives;
        jvmti->SetJNIFunctionTable(jni_functions);
        jvmti->Deallocate((unsigned char*)jni_functions);
    }
    if (env->ExceptionCheck() || _orig_unsafe_park == NULL) {
        return Error("Unsafe_park address not found");
    }

    _parkBlocker = env->GetFieldID(env->FindClass("java/lang/Thread"), "parkBlocker", "Ljava/lang/Object;");
    if (_parkBlocker == NULL) {
        return Error("parkBlocker field not found");
    }

    jclass cls = env->DefineClass(LOCK_TRACER_NAME, NULL, (const jbyte*)LOCK_TRACER_CLASS, INCBIN_SIZEOF(LOCK_TRACER_CLASS));
    if (cls != NULL) {
        const JNINativeMethod method = {(char*)"setEntry0", (char*)"(J)V", (void*)setEntry0};
        if (env->RegisterNatives(cls, &method, 1) != 0) {
            return Error("LockTracer registration failed");
        }
    } else {
        env->ExceptionClear();
        if ((cls = env->FindClass(LOCK_TRACER_NAME)) == NULL) {
            return Error("LockTracer registration failed");
        }
    }
    _LockTracer = (jclass)env->NewGlobalRef(cls);

    _setEntry = env->GetStaticMethodID(_LockTracer, "setEntry", "(J)V");
    if (_setEntry == NULL) {
        return Error("setEntry method not found");
    }

    return Error::OK;
}

void LockTracer::setUnsafeParkEntry(JNIEnv* env, UnsafeParkFunc entry) {
    if (_setEntry != NULL) {
        env->CallStaticVoidMethod(_LockTracer, _setEntry, (jlong)(uintptr_t)entry);
        env->ExceptionClear();
    }
}

void LockTracer::setEntry0(JNIEnv* env, jclass cls, jlong entry) {
    const JNINativeMethod park = {(char*)"park", (char*)"(ZJ)V", (void*)(uintptr_t)entry};
    env->RegisterNatives(_Unsafe, &park, 1);
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    const u64 enter_time = TSC::ticks();
    if (CAN_USE_TLS && lock_tracer_tls) {
        pthread_setspecific(lock_tracer_tls, (void*)enter_time);
    } else {
        jvmti->SetTag(thread, enter_time);
    }
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    if (!_enabled) return;

    const u64 entered_time = TSC::ticks();
    u64 enter_time = 0;
    if (CAN_USE_TLS && lock_tracer_tls) {
        enter_time = (u64)pthread_getspecific(lock_tracer_tls);
    } else {
        jvmti->GetTag(thread, (jlong*)&enter_time);
    }

    // Time is meaningless if lock attempt has started before profiling
    if (enter_time < _start_time) {
        return;
    }

    // When the duration accumulator overflows _interval, the event is sampled.
    const u64 duration = entered_time - enter_time;
    if (updateCounter(_total_duration, duration, _interval)) {
        char* lock_name = getLockName(jvmti, env, object);
        recordContendedLock(LOCK_SAMPLE, enter_time, entered_time, lock_name, object, 0);
        jvmti->Deallocate((unsigned char*)lock_name);
    }
}

jint JNICALL LockTracer::RegisterNativesHook(JNIEnv* env, jclass cls, const JNINativeMethod* methods, jint nMethods) {
    if (env->IsSameObject(cls, _Unsafe)) {
        for (jint i = 0; i < nMethods; i++) {
            if (strcmp(methods[i].name, "park") == 0 && strcmp(methods[i].signature, "(ZJ)V") == 0) {
                _orig_unsafe_park = (UnsafeParkFunc)methods[i].fnPtr;
                break;
            }
        }
        return 0;
    }
    return _orig_register_natives(env, cls, methods, nMethods);
}

void JNICALL LockTracer::UnsafeParkHook(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time) {
    while (_enabled) {
        jvmtiEnv* jvmti = VM::jvmti();
        jobject park_blocker = getParkBlocker(jvmti, env);
        if (park_blocker == NULL) {
            break;
        }

        char* lock_name = getLockName(jvmti, env, park_blocker);
        if (lock_name == NULL || !isConcurrentLock(lock_name)) {
            jvmti->Deallocate((unsigned char*)lock_name);
            break;
        }

        u64 park_start_time = TSC::ticks();
        _orig_unsafe_park(env, instance, isAbsolute, time);
        u64 park_end_time = TSC::ticks();

        const u64 duration = park_end_time - park_start_time;
        if (updateCounter(_total_duration, duration, _interval)) {
            recordContendedLock(PARK_SAMPLE, park_start_time, park_end_time, lock_name, park_blocker, time);
        }

        jvmti->Deallocate((unsigned char*)lock_name);
        return;
    }

    _orig_unsafe_park(env, instance, isAbsolute, time);
}

jobject LockTracer::getParkBlocker(jvmtiEnv* jvmti, JNIEnv* env) {
    jthread thread;
    if (jvmti->GetCurrentThread(&thread) != 0) {
        return NULL;
    }
    return env->GetObjectField(thread, _parkBlocker);
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
    return strncmp(lock_name, "Ljava/util/concurrent/locks/Reentrant", 37) == 0 ||
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
