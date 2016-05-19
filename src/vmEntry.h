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

#include <jvmti.h>

class VM {
  private:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;

    static void loadMethodIDs(jvmtiEnv* jvmti, jclass klass);
    static void loadAllMethodIDs(jvmtiEnv* jvmti);

  public:
    static void init(JavaVM* vm);

    static void attach(JavaVM* vm) {
        init(vm);
        loadAllMethodIDs(_jvmti);
    }

    static jvmtiEnv* jvmti() {
        return _jvmti;
    }

    static JNIEnv* jni() {
        JNIEnv* jni;
        return _vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == 0 ? jni : NULL;
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

    static void JNICALL CompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method,
                                           jint code_size, const void* code_addr,
                                           jint map_length, const jvmtiAddrLocationMap* map,
                                           const void* compile_info) {
        // Needed to enable DebugNonSafepoints info by default
    }
};
