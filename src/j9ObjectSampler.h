/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _J9OBJECTSAMPLER_H
#define _J9OBJECTSAMPLER_H

#include "objectSampler.h"


class J9ObjectSampler : public ObjectSampler {
  public:
    const char* type() {
        return "j9_object_sampler";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static void JNICALL JavaObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                        jobject object, jclass object_klass, jlong size);

    static void JNICALL VMObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                      jobject object, jclass object_klass, jlong size);
};

#endif // _J9OBJECTSAMPLER_H
