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
#include <string.h>
#include "codeCache.h"


class VMStructs {
  protected:
    static CodeCache* _libjvm;

    static bool _has_class_names;
    static bool _has_method_structs;
    static bool _has_class_loader_data;
    static bool _has_native_thread_id;
    static bool _has_perm_gen;

    static int _klass_name_offset;
    static int _symbol_length_offset;
    static int _symbol_length_and_refcount_offset;
    static int _symbol_body_offset;
    static int _class_loader_data_offset;
    static int _class_loader_data_next_offset;
    static int _methods_offset;
    static int _jmethod_ids_offset;
    static int _thread_osthread_offset;
    static int _thread_anchor_offset;
    static int _thread_state_offset;
    static int _osthread_id_offset;
    static int _anchor_sp_offset;
    static int _anchor_pc_offset;
    static int _frame_size_offset;
    static int _frame_complete_offset;
    static int _nmethod_name_offset;
    static int _nmethod_method_offset;
    static int _nmethod_entry_offset;
    static int _nmethod_state_offset;
    static int _nmethod_level_offset;
    static int _method_constmethod_offset;
    static int _method_code_offset;
    static int _constmethod_constants_offset;
    static int _constmethod_idnum_offset;
    static int _pool_holder_offset;
    static int _array_data_offset;
    static int _code_heap_memory_offset;
    static int _code_heap_segmap_offset;
    static int _code_heap_segment_shift;
    static int _vs_low_bound_offset;
    static int _vs_high_bound_offset;
    static int _vs_low_offset;
    static int _vs_high_offset;
    static int _flag_name_offset;
    static int _flag_addr_offset;
    static const char* _flags_addr;
    static int _flag_count;
    static int _flag_size;
    static char* _code_heap[3];
    static const void* _code_heap_low;
    static const void* _code_heap_high;
    static char** _code_heap_addr;
    static const void** _code_heap_low_addr;
    static const void** _code_heap_high_addr;
    static int* _klass_offset_addr;

    static jfieldID _eetop;
    static jfieldID _tid;
    static jfieldID _klass;
    static int _tls_index;
    static intptr_t _env_offset;

    typedef void (*LockFunc)(void*);
    static LockFunc _lock_func;
    static LockFunc _unlock_func;

    static uintptr_t readSymbol(const char* symbol_name);
    static void initOffsets();
    static void resolveOffsets();
    static void initJvmFunctions();
    static void initTLS(void* vm_thread);
    static void initThreadBridge(JNIEnv* env);
    static void initLogging(JNIEnv* env);

    const char* at(int offset) {
        return (const char*)this + offset;
    }

  public:
    static void init(CodeCache* libjvm);
    static void ready();

    static CodeCache* libjvm() {
        return _libjvm;
    }

    static bool hasClassNames() {
        return _has_class_names;
    }

    static bool hasMethodStructs() {
        return _has_method_structs;
    }

    static bool hasClassLoaderData() {
        return _has_class_loader_data;
    }

    static bool hasJavaThreadId() {
        return _tid != NULL;
    }

    typedef jvmtiError (*GetStackTraceFunc)(void* self, void* thread,
                                            jint start_depth, jint max_frame_count,
                                            jvmtiFrameInfo* frame_buffer, jint* count_ptr);
    static GetStackTraceFunc _get_stack_trace;
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


class NMethod;

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

    jmethodID* jmethodIDs() {
        return __atomic_load_n((jmethodID**) at(_jmethod_ids_offset), __ATOMIC_ACQUIRE);
    }
};

class VMThread : VMStructs {
  public:
    static VMThread* current();

    static int key() {
        return _tls_index;
    }

    static VMThread* fromJavaThread(JNIEnv* env, jthread thread) {
        return (VMThread*)(uintptr_t)env->GetLongField(thread, _eetop);
    }

    static VMThread* fromEnv(JNIEnv* env) {
        return (VMThread*)((intptr_t)env - _env_offset);
    }

    static jlong javaThreadId(JNIEnv* env, jthread thread) {
        return env->GetLongField(thread, _tid);
    }

    static int nativeThreadId(JNIEnv* jni, jthread thread);

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

class ConstMethod : VMStructs {
  public:
    jmethodID id();
};

class VMMethod : VMStructs {
  public:
    static VMMethod* fromMethodID(jmethodID id) {
        return *(VMMethod**)id;
    }

    ConstMethod* constMethod() {
        return *(ConstMethod**) at(_method_constmethod_offset);
    }

    NMethod* code() {
        return *(NMethod**) at(_method_code_offset);
    }
};

class NMethod : VMStructs {
  public:
    int frameSize() {
        return *(int*) at(_frame_size_offset);
    }

    int frameCompleteOffset() {
        return *(int*) at(_frame_complete_offset);
    }

    void setFrameCompleteOffset(int offset) {
        *(int*) at(_frame_complete_offset) = offset;
    }

    const char* name() {
        return *(const char**) at(_nmethod_name_offset);
    }

    bool isNMethod() {
        const char* n = name();
        return n != NULL && (strcmp(n, "nmethod") == 0 || strcmp(n, "native nmethod") == 0);
    }

    bool isInterpreter() {
        const char* n = name();
        return n != NULL && strcmp(n, "Interpreter") == 0;
    }

    VMMethod* method() {
        return *(VMMethod**) at(_nmethod_method_offset);
    }

    void* entry() {
        return *(void**) at(_nmethod_entry_offset);
    }

    char state() {
        return *at(_nmethod_state_offset);
    }

    bool isAlive() {
        return state() >= 0 && state() <= 1;
    }

    int level() {
        return _nmethod_level_offset >= 0 ? *(int*) at(_nmethod_level_offset) : 0;
    }
};

class CodeHeap : VMStructs {
  private:
    static bool contains(char* heap, const void* pc) {
        return heap != NULL &&
               pc >= *(const void**)(heap + _code_heap_memory_offset + _vs_low_offset) &&
               pc <  *(const void**)(heap + _code_heap_memory_offset + _vs_high_offset);
    }

    static NMethod* findNMethod(char* heap, const void* pc);

  public:
    static bool available() {
        return _code_heap_addr != NULL;
    }

    static bool contains(const void* pc) {
        return _code_heap_low <= pc && pc < _code_heap_high;
    }

    static void updateBounds(const void* start, const void* end) {
        for (const void* low = _code_heap_low;
             start < low && !__sync_bool_compare_and_swap(&_code_heap_low, low, start);
             low = _code_heap_low);
        for (const void* high = _code_heap_high;
             end > high && !__sync_bool_compare_and_swap(&_code_heap_high, high, end);
             high = _code_heap_high);
    }

    static NMethod* findNMethod(const void* pc) {
        if (contains(_code_heap[0], pc)) return findNMethod(_code_heap[0], pc);
        if (contains(_code_heap[1], pc)) return findNMethod(_code_heap[1], pc);
        if (contains(_code_heap[2], pc)) return findNMethod(_code_heap[2], pc);
        return NULL;
    }
};

class JVMFlag : VMStructs {
  public:
    static void* find(const char* name);

    const char* name() {
        return *(const char**) at(_flag_name_offset);
    }

    void* addr() {
        return *(void**) at(_flag_addr_offset);
    }
};

#endif // _VMSTRUCTS_H
