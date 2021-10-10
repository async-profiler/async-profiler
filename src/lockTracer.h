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

#ifndef _LOCKTRACER_H
#define _LOCKTRACER_H

#include <jvmti.h>
#include "arch.h"
#include "engine.h"


typedef jint (JNICALL *RegisterNativesFunc)(JNIEnv*, jclass, const JNINativeMethod*, jint);
typedef void (JNICALL *UnsafeParkFunc)(JNIEnv*, jobject, jboolean, jlong);

class LockTracer : public Engine {
  private:
    static double _ticks_to_nanos;
    static jlong _threshold;
    static jlong _start_time;
    static jclass _UnsafeClass;
    static jclass _LockSupport;
    static jmethodID _getBlocker;
    static bool _initialized;

    static void initialize();

    static RegisterNativesFunc _orig_RegisterNatives;
    static jint JNICALL RegisterNativesHook(JNIEnv* env, jclass cls, const JNINativeMethod* methods, jint nMethods);

    static UnsafeParkFunc _orig_Unsafe_park;
    static void JNICALL UnsafeParkHook(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time);

    static jobject getParkBlocker(jvmtiEnv* jvmti, JNIEnv* env);
    static char* getLockName(jvmtiEnv* jvmti, JNIEnv* env, jobject lock);
    static bool isConcurrentLock(const char* lock_name);
    static void recordContendedLock(int event_type, u64 start_time, u64 end_time,
                                    const char* lock_name, jobject lock, jlong timeout);
    static void bindUnsafePark(UnsafeParkFunc entry);

  public:
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
