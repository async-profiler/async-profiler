/*
 * Copyright 2022 Andrei Pangin
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

#ifndef _J9OBJECTSAMPLER_H
#define _J9OBJECTSAMPLER_H

#include "objectSampler.h"


class J9ObjectSampler : public ObjectSampler {
  public:
    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static void JNICALL JavaObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                        jobject object, jclass object_klass, jlong size);

    static void JNICALL VMObjectAlloc(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread,
                                      jobject object, jclass object_klass, jlong size);
};

#endif // _J9OBJECTSAMPLER_H
