/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VMENTRY_H
#define _VMENTRY_H

#include <jvmti.h>


enum FrameTypeId {
    FRAME_INTERPRETED  = 0,
    FRAME_JIT_COMPILED = 1,
    FRAME_INLINED      = 2,
    FRAME_NATIVE       = 3,
    FRAME_CPP          = 4,
    FRAME_KERNEL       = 5,
    FRAME_C1_COMPILED  = 6,
};

class FrameType {
  public:
    static inline int encode(int type, int bci) {
        return (1 << 24) | (type << 25) | (bci & 0xffffff);
    }

    static inline FrameTypeId decode(int bci) {
        return (bci >> 24) > 0 ? (FrameTypeId)(bci >> 25) : FRAME_JIT_COMPILED;
    }
};


// Denotes ASGCT_CallFrame where method_id has special meaning (not jmethodID)
enum ASGCT_CallFrameType {
    BCI_NATIVE_FRAME        = -10,  // native function name (char*)
    BCI_ALLOC               = -11,  // name of the allocated class
    BCI_ALLOC_OUTSIDE_TLAB  = -12,  // name of the class allocated outside TLAB
    BCI_LIVE_OBJECT         = -13,  // name of the allocated class
    BCI_LOCK                = -14,  // class name of the locked object
    BCI_PARK                = -15,  // class name of the park() blocker
    BCI_THREAD_ID           = -16,  // method_id designates a thread
    BCI_ADDRESS             = -17,  // method_id is a PC address
    BCI_ERROR               = -18,  // method_id is an error string
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

typedef jlong (*JVM_MemoryFunc)();

typedef struct {
    void* unused1[86];
    jvmtiError (JNICALL *RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
    void* unused2[64];
    jvmtiError (JNICALL *RetransformClasses)(jvmtiEnv*, jint, const jclass*);
} JVMTIFunctions;


class VM {
  private:
    static JavaVM* _vm;
    static jvmtiEnv* _jvmti;

    static int _hotspot_version;
    static bool _openj9;
    static bool _zing;

    static jvmtiError (JNICALL *_orig_RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
    static jvmtiError (JNICALL *_orig_RetransformClasses)(jvmtiEnv*, jint, const jclass* classes);

    static void ready();
    static void applyPatch(char* func, const char* patch, const char* end_patch);
    static void loadMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni, jclass klass);
    static void loadAllMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni);

  public:
    static AsyncGetCallTrace _asyncGetCallTrace;
    static JVM_MemoryFunc _totalMemory;
    static JVM_MemoryFunc _freeMemory;

    static bool init(JavaVM* vm, bool attach);

    static bool loaded() {
        return _jvmti != NULL;
    }

    static jvmtiEnv* jvmti() {
        return _jvmti;
    }

    static JNIEnv* jni() {
        JNIEnv* jni;
        return _vm && _vm->GetEnv((void**)&jni, JNI_VERSION_1_6) == 0 ? jni : NULL;
    }

    static JNIEnv* attachThread(const char* name) {
        JNIEnv* jni;
        JavaVMAttachArgs args = {JNI_VERSION_1_6, (char*)name, NULL};
        return _vm->AttachCurrentThreadAsDaemon((void**)&jni, &args) == 0 ? jni : NULL;
    }

    static void detachThread() {
        _vm->DetachCurrentThread();
    }

    static int hotspot_version() {
        return _hotspot_version;
    }

    static bool isOpenJ9() {
        return _openj9;
    }

    static bool isZing() {
        return _zing;
    }

    static bool addSampleObjectsCapability() {
        jvmtiCapabilities capabilities = {0};
        capabilities.can_generate_sampled_object_alloc_events = 1;
        return _jvmti->AddCapabilities(&capabilities) == 0;
    }

    static void releaseSampleObjectsCapability() {
        jvmtiCapabilities capabilities = {0};
        capabilities.can_generate_sampled_object_alloc_events = 1;
        _jvmti->RelinquishCapabilities(&capabilities);
    }

    static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    static void JNICALL VMDeath(jvmtiEnv* jvmti, JNIEnv* jni);

    static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        // Needed only for AsyncGetCallTrace support
    }

    static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
        loadMethodIDs(jvmti, jni, klass);
    }

    static jvmtiError JNICALL RedefineClassesHook(jvmtiEnv* jvmti, jint class_count, const jvmtiClassDefinition* class_definitions);
    static jvmtiError JNICALL RetransformClassesHook(jvmtiEnv* jvmti, jint class_count, const jclass* classes);
};

#endif // _VMENTRY_H
