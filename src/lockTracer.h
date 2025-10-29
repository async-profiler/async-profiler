/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LOCKTRACER_H
#define _LOCKTRACER_H

#include <jvmti.h>
#include "arch.h"
#include "engine.h"
#include "event.h"


typedef jint (JNICALL *RegisterNativesFunc)(JNIEnv*, jclass, const JNINativeMethod*, jint);
typedef void (JNICALL *UnsafeParkFunc)(JNIEnv*, jobject, jboolean, jlong);

class LockTracer : public Engine {
  private:
    static bool _initialized;
    static double _ticks_to_nanos;
    static u64 _interval;
    static volatile u64 _total_duration;
    static u64 _start_time;

    static jclass _Unsafe;
    static jclass _LockTracer;
    static jfieldID _parkBlocker;
    static jmethodID _setEntry;

    static Error initialize(jvmtiEnv* jvmti, JNIEnv* env);

    static void setUnsafeParkEntry(JNIEnv* env, UnsafeParkFunc entry);
    static void setEntry0(JNIEnv* env, jclass cls, jlong entry);

    static RegisterNativesFunc _orig_register_natives;
    static jint JNICALL RegisterNativesHook(JNIEnv* env, jclass cls, const JNINativeMethod* methods, jint nMethods);

    static UnsafeParkFunc _orig_unsafe_park;
    static void JNICALL UnsafeParkHook(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time);

    static jobject getParkBlocker(jvmtiEnv* jvmti, JNIEnv* env);
    static char* getLockName(jvmtiEnv* jvmti, JNIEnv* env, jobject lock);
    static bool isConcurrentLock(const char* lock_name);

    static void recordContendedLock(EventType event_type, u64 start_time, u64 end_time,
                                    const char* lock_name, jobject lock, jlong timeout);

  public:
    const char* type() {
        return "lock_tracer";
    }

    const char* title() {
        return "Lock profile";
    }

    const char* units() {
        return "ns";
    }

    Error start(Arguments& args);
    void stop();

    static void JNICALL MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object);
    static void JNICALL MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object);
};

#endif // _LOCKTRACER_H
