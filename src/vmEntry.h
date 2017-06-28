/*
 * Copyright 2016 Andrei Pangin
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

#ifndef _VMENTRY_H
#define _VMENTRY_H

#include <jvmti.h>


// Denotes ASGCT_CallFrame with string name instead of Java method ID
const jint BCI_NATIVE_FRAME = -2;

typedef struct {
    jint bci;
    jmethodID method_id;
} ASGCT_CallFrame;

typedef struct {
    JNIEnv* env;
    jint num_frames;
    ASGCT_CallFrame* frames;
} ASGCT_CallTrace;

typedef void (*AsyncGetCallTrace)(ASGCT_CallTrace*, jint, void*);


class VM {
  private:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;
    static AsyncGetCallTrace _asyncGetCallTrace;

    static void loadMethodIDs(jvmtiEnv* jvmti, jclass klass);
    static void loadAllMethodIDs(jvmtiEnv* jvmti);

  public:
    static bool init(JavaVM* vm);
    static bool attach(JavaVM* vm);

    static jvmtiEnv* jvmti() {
        return _jvmti;
    }

    static JNIEnv* jni() {
        JNIEnv* jni;
        return _vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == 0 ? jni : NULL;
    }

    static void asyncGetCallTrace(ASGCT_CallTrace* trace, jint depth, void* ucontext) {
        _asyncGetCallTrace(trace, depth, ucontext);
    }

    static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
        loadAllMethodIDs(jvmti);
    }

    static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        // Needed only for AsyncGetCallTrace support
    }

    static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        loadMethodIDs(jvmti, klass);
    }
};

#endif // _VMENTRY_H
