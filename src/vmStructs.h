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

#ifndef _VMSTRUCTS_H
#define _VMSTRUCTS_H

#include <jvmti.h>
#include <stdint.h>
#include "codeCache.h"


class VMStructs {
  protected:
    static NativeCodeCache* _libjvm;

    static bool _has_class_names;
    static bool _has_class_loader_data;
    static bool _has_thread_bridge;
    static bool _has_perm_gen;

    static int _klass_name_offset;
    static int _symbol_length_offset;
    static int _symbol_length_and_refcount_offset;
    static int _symbol_body_offset;
    static int _class_loader_data_offset;
    static int _methods_offset;
    static int _thread_osthread_offset;
    static int _thread_anchor_offset;
    static int _thread_state_offset;
    static int _osthread_id_offset;
    static int _anchor_sp_offset;
    static int _anchor_pc_offset;
    static int _frame_size_offset;
    static int _is_gc_active_offset;
    static char* _collected_heap_addr;
    static const void* _code_heap_low;
    static const void* _code_heap_high;

    static jfieldID _eetop;
    static jfieldID _tid;
    static jfieldID _klass;
    static int _tls_index;
    static intptr_t _env_offset;

    typedef void* (*FindBlobFunc)(const void*);
    static FindBlobFunc _find_blob;

    typedef void (*LockFunc)(void*);
    static LockFunc _lock_func;
    static LockFunc _unlock_func;

    static char* _method_flushing;
    static int* _sweep_started;

    static uintptr_t readSymbol(const char* symbol_name);
    static void initOffsets();
    static void initJvmFunctions();
    static void initThreadBridge(JNIEnv* env);
    static void initLogging(JNIEnv* env);

    const char* at(int offset) {
        return (const char*)this + offset;
    }

  public:
    static void init(NativeCodeCache* libjvm);

    static NativeCodeCache* libjvm() {
        return _libjvm;
    }

    static bool hasClassNames() {
        return _has_class_names;
    }

    static bool hasClassLoaderData() {
        return _has_class_loader_data;
    }

    static bool hasThreadBridge() {
        return _has_thread_bridge;
    }

    typedef jvmtiError (*GetStackTraceFunc)(void* self, void* thread,
                                            jint start_depth, jint max_frame_count,
                                            jvmtiFrameInfo* frame_buffer, jint* count_ptr);
    static GetStackTraceFunc _get_stack_trace;

    typedef void (JNICALL *UnsafeParkFunc)(JNIEnv*, jobject, jboolean, jlong);
    static UnsafeParkFunc _unsafe_park;
};


class MethodList {
  public:
    enum { SIZE = 8 };

  private:
    intptr_t _method[SIZE];
    int _ptr;
    MethodList* _next;
    int _padding;

  public:
    MethodList(MethodList* next) : _ptr(0), _next(next), _padding(0) {
        for (int i = 0; i < SIZE; i++) {
            _method[i] = 0x37;
        }
    }
};


class VMSymbol : VMStructs {
  public:
    unsigned short length() {
        if (_symbol_length_offset >= 0) {
          return *(unsigned short*) at(_symbol_length_offset);
        } else {
          return *(unsigned int*) at(_symbol_length_and_refcount_offset) >> 16;
        }
    }

    const char* body() {
        return at(_symbol_body_offset);
    }
};

class ClassLoaderData : VMStructs {
  private:
    void* mutex() {
        return *(void**) at(sizeof(uintptr_t) * 3);
    }

  public:
    void lock() {
        _lock_func(mutex());
    }

    void unlock() {
        _unlock_func(mutex());
    }

    MethodList** methodList() {
        return (MethodList**) at(sizeof(uintptr_t) * 6 + 8);
    }
};

class VMKlass : VMStructs {
  public:
    static VMKlass* fromJavaClass(JNIEnv* env, jclass cls) {
        if (_has_perm_gen) {
            jobject klassOop = env->GetObjectField(cls, _klass);
            return (VMKlass*)(*(uintptr_t**)klassOop + 2);
        } else if (sizeof(VMKlass*) == 8) {
            return (VMKlass*)(uintptr_t)env->GetLongField(cls, _klass);
        } else {
            return (VMKlass*)(uintptr_t)env->GetIntField(cls, _klass);
        }
    }

    static VMKlass* fromHandle(uintptr_t handle) {
        if (_has_perm_gen) {
            // On JDK 7 KlassHandle is a pointer to klassOop, hence one more indirection
            return (VMKlass*)(*(uintptr_t**)handle + 2);
        } else {
            return (VMKlass*)handle;
        }
    }

    VMSymbol* name() {
        return *(VMSymbol**) at(_klass_name_offset);
    }

    ClassLoaderData* classLoaderData() {
        return *(ClassLoaderData**) at(_class_loader_data_offset);
    }

    int methodCount() {
        int* methods = *(int**) at(_methods_offset);
        return methods == NULL ? 0 : *methods & 0xffff;
    }
};

class VMThread : VMStructs {
  public:
    static VMThread* current();

    static VMThread* fromJavaThread(JNIEnv* env, jthread thread) {
        return (VMThread*)(uintptr_t)env->GetLongField(thread, _eetop);
    }

    static VMThread* fromEnv(JNIEnv* env) {
        return (VMThread*)((intptr_t)env - _env_offset);
    }

    static jlong javaThreadId(JNIEnv* env, jthread thread) {
        return env->GetLongField(thread, _tid);
    }

    static bool hasNativeId() {
        return _thread_osthread_offset >= 0 && _osthread_id_offset >= 0;
    }

    int osThreadId() {
        const char* osthread = *(const char**) at(_thread_osthread_offset);
        return *(int*)(osthread + _osthread_id_offset);
    }

    int state() {
        return _thread_state_offset >= 0 ? *(int*) at(_thread_state_offset) : 0;
    }

    uintptr_t& lastJavaSP() {
        return *(uintptr_t*) (at(_thread_anchor_offset) + _anchor_sp_offset);
    }

    uintptr_t& lastJavaPC() {
        return *(uintptr_t*) (at(_thread_anchor_offset) + _anchor_pc_offset);
    }
};

class RuntimeStub : VMStructs {
  public:
    static RuntimeStub* findBlob(const void* pc) {
        return _find_blob != NULL ? (RuntimeStub*)_find_blob(pc) : NULL;
    }

    int frameSize() {
        return *(int*) at(_frame_size_offset);
    }
};

class CodeHeap : VMStructs {
  public:
    static bool contains(const void* pc) {
        return _code_heap_low <= pc && pc < _code_heap_high;
    }

    static void updateBounds(const void* start, const void* end) {
        if (start < _code_heap_low) _code_heap_low = start;
        if (end > _code_heap_high) _code_heap_high = end;
    }
};

class CollectedHeap : VMStructs {
  public:
    static bool isGCActive() {
        return _collected_heap_addr != NULL && _is_gc_active_offset >= 0 &&
               _collected_heap_addr[_is_gc_active_offset] != 0;
    }
};

class DisableSweeper : VMStructs {
  private:
    bool _enabled;

  public:
    DisableSweeper();
    ~DisableSweeper();
};

#endif // _VMSTRUCTS_H
