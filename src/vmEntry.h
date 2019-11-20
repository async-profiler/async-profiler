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


// ASGCT_CallFrames where method_id has special meaning (not jmethodID) are distinguished by the bci field
enum ASGCT_CallFrameType {
    BCI_SMALLEST_USED_BY_VM = -9,      // small negative BCIs are used by the VM (-6 is the smallest currently)
    BCI_NATIVE_FRAME        = -10,     // method_id is native function name (char*)
    BCI_SYMBOL              = -11,     // method_id is VMSymbol*
    BCI_SYMBOL_OUTSIDE_TLAB = -12,     // VMSymbol* specifically for allocations outside TLAB
    BCI_THREAD_ID           = -13,     // method_id designates a thread
    BCI_ERROR               = -14,     // method_id is error string
    BCI_KERNEL_FRAME        = -15,     // method_id is native function name (char*) in the OS kernel
    BCI_OFFSET_COMP         = 0x10000, // offset added to bci for compiled java method
    BCI_OFFSET_INTERP       = 0x20000, // offset added to bci for interpreted java method
    BCI_OFFSET_INLINED      = 0x30000, // offset added to bci for inlined java method
};

// See hotspot/src/share/vm/prims/forte.cpp
enum ASGCT_Failure {
    ticks_no_Java_frame         =  0,
    ticks_no_class_load         = -1,
    ticks_GC_active             = -2,
    ticks_unknown_not_Java      = -3,
    ticks_not_walkable_not_Java = -4,
    ticks_unknown_Java          = -5,
    ticks_not_walkable_Java     = -6,
    ticks_unknown_state         = -7,
    ticks_thread_exit           = -8,
    ticks_deopt                 = -9,
    ticks_safepoint             = -10,
    ticks_skipped               = -11,
    ASGCT_FAILURE_TYPES         = 12
};

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

// Frame types used for output (output generators use these directly)
enum StoredFrameType {
    FRAME_TYPE_NATIVE           = 'n',
    FRAME_TYPE_KERNEL           = 'k',
    FRAME_TYPE_VMSYM            = 'v',
    FRAME_TYPE_OUTSIDE_TLAB     = 'o',
    FRAME_TYPE_THREAD           = 't',
    FRAME_TYPE_COMPILED_JAVA    = 'J',
    FRAME_TYPE_INTERPRETED_JAVA = 'I',
    FRAME_TYPE_INLINED_JAVA     = 'i',
    FRAME_TYPE_UNKNOWN_JAVA     = 'j',
    FRAME_TYPE_CPP              = 'p',
    FRAME_TYPE_BOTTOM           = 'b',
    FRAME_TYPE_ERROR            = 'e',
};

class VM {
  private:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;
    static bool _hotspot;

    static void* getLibraryHandle(const char* name);
    static void loadMethodIDs(jvmtiEnv* jvmti, jclass klass);
    static void loadAllMethodIDs(jvmtiEnv* jvmti);

  public:
    static void* _libjvm;
    static void* _libjava;
    static AsyncGetCallTrace _asyncGetCallTrace;

    static void init(JavaVM* vm, bool attach);

    static jvmtiEnv* jvmti() {
        return _jvmti;
    }

    static JNIEnv* jni() {
        JNIEnv* jni;
        return _vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == 0 ? jni : NULL;
    }

    static bool is_hotspot() {
        return _hotspot;
    }

    static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    static void JNICALL VMDeath(jvmtiEnv* jvmti, JNIEnv* jni);

    static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        // Needed only for AsyncGetCallTrace support
    }

    static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        loadMethodIDs(jvmti, klass);
    }
};

#endif // _VMENTRY_H
