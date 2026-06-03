/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _JAVAAPI_H
#define _JAVAAPI_H

#include <jvmti.h>


class JavaAPI {
  public:
    static void registerNatives(jvmtiEnv* jvmti, JNIEnv* jni);
    static bool startHttpServer(jvmtiEnv* jvmti, JNIEnv* jni, const char* address);
};

class RecordingAPI {
  private:
    static jclass _recording_class;
    static jfieldID _state_field;
    static jmethodID _update_clock_method;
    static bool _tsc_enabled;

    static void updateClock(JNIEnv* env);

  public:
    // Should match constants in Recording.java
    enum State {
        UNAVAILABLE,
        STOPPED,
        RUNNING
    };

    static State registerNatives(JNIEnv* env, jclass recording_class);
    static void bind(jvmtiEnv* jvmti, JNIEnv* env);
    static void start();
    static void stop();
};

#endif // _JAVAAPI_H
