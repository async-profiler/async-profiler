/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _J9EXT_H
#define _J9EXT_H

#include <jvmti.h>


#define JVMTI_EXT(f, ...)  ((jvmtiError (*)(jvmtiEnv*, __VA_ARGS__))f)

struct jvmtiFrameInfoExtended {
    jmethodID method;
    jlocation location;
    jlocation machinepc;
    jint type;
    void* native_frame_address;
};

struct jvmtiStackInfoExtended {
    jthread thread;
    jint state;
    jvmtiFrameInfoExtended* frame_buffer;
    jint frame_count;
};

enum {
    SHOW_COMPILED_FRAMES = 4,
    SHOW_INLINED_FRAMES = 8
};


class J9Ext {
  private:
    static jvmtiEnv* _jvmti;

    static void* (*_j9thread_self)();

    static jvmtiExtensionFunction _GetOSThreadID;
    static jvmtiExtensionFunction _GetJ9vmThread;
    static jvmtiExtensionFunction _GetStackTraceExtended;
    static jvmtiExtensionFunction _GetAllStackTracesExtended;

  public:
    static bool initialize(jvmtiEnv* jvmti, const void* j9thread_self);

    static int GetOSThreadID(jthread thread) {
        jlong thread_id;
        return JVMTI_EXT(_GetOSThreadID, jthread, jlong*)(_jvmti, thread, &thread_id) == 0 ? (int)thread_id : -1;
    }

    static JNIEnv* GetJ9vmThread(jthread thread) {
        JNIEnv* result;
        return JVMTI_EXT(_GetJ9vmThread, jthread, JNIEnv**)(_jvmti, thread, &result) == 0 ? result : NULL;
    }

    static jvmtiError GetStackTraceExtended(jthread thread, jint start_depth, jint max_frame_count,
                                            void* frame_buffer, jint* count_ptr) {
        return JVMTI_EXT(_GetStackTraceExtended, jint, jthread, jint, jint, void*, jint*)(
            _jvmti, SHOW_COMPILED_FRAMES | SHOW_INLINED_FRAMES,
            thread, start_depth, max_frame_count, frame_buffer, count_ptr);
    }

    static jvmtiError GetAllStackTracesExtended(jint max_frame_count, void** stack_info_ptr, jint* thread_count_ptr) {
        return JVMTI_EXT(_GetAllStackTracesExtended, jint, jint, void**, jint*)(
            _jvmti, SHOW_COMPILED_FRAMES | SHOW_INLINED_FRAMES,
            max_frame_count, stack_info_ptr, thread_count_ptr);
    }

    static void* j9thread_self() {
        return _j9thread_self != NULL ? _j9thread_self() : NULL;
    }

    static int InstrumentableObjectAlloc_id;
};


#endif // _J9EXT_H
