/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _VMSTRUCTS_H
#define _VMSTRUCTS_H

#include <jvmti.h>
#include <stdint.h>
#include <string.h>
#include "codeCache.h"


class VMStructs {
  protected:
    enum { MONITOR_BIT = 2 };

    static CodeCache* _libjvm;

    static bool _has_class_names;
    static bool _has_method_structs;
    static bool _has_compiler_structs;
    static bool _has_stack_structs;
    static bool _has_class_loader_data;
    static bool _has_native_thread_id;
    static bool _has_perm_gen;
    static bool _compact_object_headers;

    static int _klass_name_offset;
    static int _symbol_length_offset;
    static int _symbol_length_and_refcount_offset;
    static int _symbol_body_offset;
    static int _oop_klass_offset;
    static int _class_loader_data_offset;
    static int _class_loader_data_next_offset;
    static int _methods_offset;
    static int _jmethod_ids_offset;
    static int _thread_osthread_offset;
    static int _thread_anchor_offset;
    static int _thread_state_offset;
    static int _thread_vframe_offset;
    static int _thread_exception_offset;
    static int _osthread_id_offset;
    static int _comp_env_offset;
    static int _comp_task_offset;
    static int _comp_method_offset;
    static int _anchor_sp_offset;
    static int _anchor_pc_offset;
    static int _anchor_fp_offset;
    static int _frame_size_offset;
    static int _frame_complete_offset;
    static int _code_offset;
    static int _data_offset;
    static int _scopes_pcs_offset;
    static int _scopes_data_offset;
    static int _nmethod_name_offset;
    static int _nmethod_method_offset;
    static int _nmethod_entry_offset;
    static int _nmethod_state_offset;
    static int _nmethod_level_offset;
    static int _nmethod_metadata_offset;
    static int _nmethod_immutable_offset;
    static int _method_constmethod_offset;
    static int _method_code_offset;
    static int _constmethod_constants_offset;
    static int _constmethod_idnum_offset;
    static int _constmethod_size;
    static int _pool_holder_offset;
    static int _array_len_offset;
    static int _array_data_offset;
    static int _code_heap_memory_offset;
    static int _code_heap_segmap_offset;
    static int _code_heap_segment_shift;
    static int _heap_block_used_offset;
    static int _vs_low_bound_offset;
    static int _vs_high_bound_offset;
    static int _vs_low_offset;
    static int _vs_high_offset;
    static int _flag_name_offset;
    static int _flag_addr_offset;
    static int _flag_origin_offset;
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
    static char** _narrow_klass_base_addr;
    static char* _narrow_klass_base;
    static int* _narrow_klass_shift_addr;
    static int _narrow_klass_shift;
    static char** _collected_heap_addr;
    static char* _collected_heap;
    static int _collected_heap_reserved_offset;
    static int _region_start_offset;
    static int _region_size_offset;
    static int _markword_klass_shift;
    static int _markword_monitor_value;
    static int _interpreter_frame_bcp_offset;
    static unsigned char _unsigned5_base;
    static const void** _call_stub_return_addr;
    static const void* _call_stub_return;
    static const void* _interpreted_frame_valid_start;
    static const void* _interpreted_frame_valid_end;

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
    static void patchSafeFetch();
    static void initJvmFunctions();
    static void initTLS(void* vm_thread);
    static void initThreadBridge();

    const char* at(int offset) {
        return (const char*)this + offset;
    }

    static bool goodPtr(const void* ptr) {
        return (uintptr_t)ptr >= 0x1000 && ((uintptr_t)ptr & (sizeof(uintptr_t) - 1)) == 0;
    }

    template<typename T>
    static T align(const void* ptr) {
        return (T)((uintptr_t)ptr & ~(sizeof(T) - 1));
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

    static bool hasCompilerStructs() {
        return _has_compiler_structs;
    }

    static bool hasStackStructs() {
        return _has_stack_structs;
    }

    static bool hasClassLoaderData() {
        return _has_class_loader_data;
    }

    static bool hasNativeThreadId() {
        return _has_native_thread_id;
    }

    static bool hasJavaThreadId() {
        return _tid != NULL;
    }

    static bool isInterpretedFrameValidFunc(const void* pc) {
        return pc >= _interpreted_frame_valid_start && pc < _interpreted_frame_valid_end;
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
class VMMethod;

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

    static VMKlass* fromOop(uintptr_t oop) {
        if (_narrow_klass_shift >= 0) {
            uintptr_t narrow_klass;
            if (_compact_object_headers) {
                uintptr_t mark = *(uintptr_t*)oop;
                if (mark & MONITOR_BIT) {
                    mark = *(uintptr_t*)(mark ^ MONITOR_BIT);
                }
                narrow_klass = mark >> _markword_klass_shift;
            } else {
                narrow_klass = *(unsigned int*)(oop + _oop_klass_offset);
            }
            return (VMKlass*)(_narrow_klass_base + (narrow_klass << _narrow_klass_shift));
        } else {
            return *(VMKlass**)(oop + _oop_klass_offset);
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
        return osthread != NULL ? *(int*)(osthread + _osthread_id_offset) : -1;
    }

    int state() {
        return _thread_state_offset >= 0 ? *(int*) at(_thread_state_offset) : 0;
    }

    bool inJava() {
        return state() == 8;
    }

    bool inDeopt() {
        return *(void**) at(_thread_vframe_offset) != NULL;
    }

    void*& exception() {
        return *(void**) at(_thread_exception_offset);
    }

    uintptr_t& lastJavaSP() {
        return *(uintptr_t*) (at(_thread_anchor_offset) + _anchor_sp_offset);
    }

    uintptr_t& lastJavaPC() {
        return *(uintptr_t*) (at(_thread_anchor_offset) + _anchor_pc_offset);
    }

    uintptr_t& lastJavaFP() {
        return *(uintptr_t*) (at(_thread_anchor_offset) + _anchor_fp_offset);
    }

    VMMethod* compiledMethod() {
        const char* env = *(const char**) at(_comp_env_offset);
        if (env != NULL) {
            const char* task = *(const char**) (env + _comp_task_offset);
            if (task != NULL) {
                return *(VMMethod**) (task + _comp_method_offset);
            }
        }
        return NULL;
    }
};

class VMMethod : VMStructs {
  public:
    static VMMethod* fromMethodID(jmethodID id) {
        return *(VMMethod**)id;
    }

    jmethodID id();

    const char* bytecode() {
        return *(const char**) at(_method_constmethod_offset) + _constmethod_size;
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

    short frameCompleteOffset() {
        return *(short*) at(_frame_complete_offset);
    }

    void setFrameCompleteOffset(int offset) {
        if (_nmethod_immutable_offset > 0) {
            // _frame_complete_offset is short on JDK 23+
            *(short*) at(_frame_complete_offset) = offset;
        } else {
            *(int*) at(_frame_complete_offset) = offset;
        }
    }

    const char* immutableDataAt(int offset) {
        if (_nmethod_immutable_offset > 0) {
            return *(const char**) at(_nmethod_immutable_offset) + offset;
        }
        return at(offset);
    }

    const char* code() {
        if (_code_offset > 0) {
            return at(*(int*) at(_code_offset));
        } else {
            return *(const char**) at(-_code_offset);
        }
    }

    const char* scopes() {
        if (_scopes_data_offset > 0) {
            return immutableDataAt(*(int*) at(_scopes_data_offset));
        } else {
            return *(const char**) at(-_scopes_data_offset);
        }
    }

    const void* entry() {
        if (_nmethod_entry_offset > 0) {
            return at(*(int*) at(_code_offset) + *(unsigned short*) at(_nmethod_entry_offset));
        } else {
            return *(void**) at(-_nmethod_entry_offset);
        }
    }

    bool isFrameCompleteAt(const void* pc) {
        return pc >= code() + frameCompleteOffset();
    }

    bool isEntryFrame(const void* pc) {
        return pc == _call_stub_return;
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

    char state() {
        return *at(_nmethod_state_offset);
    }

    bool isAlive() {
        return state() >= 0 && state() <= 1;
    }

    int level() {
        return _nmethod_level_offset >= 0 ? *(signed char*) at(_nmethod_level_offset) : 0;
    }

    VMMethod** metadata() {
        if (_data_offset > 0) {
            return (VMMethod**) at(*(int*) at(_data_offset) + *(unsigned short*) at(_nmethod_metadata_offset));
        }
        return (VMMethod**) at(*(int*) at(_nmethod_metadata_offset));
    }

    int findScopeOffset(const void* pc);
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

class CollectedHeap : VMStructs {
  public:
    static CollectedHeap* heap() {
        return (CollectedHeap*)_collected_heap;
    }

    uintptr_t start() {
        return *(uintptr_t*) at(_region_start_offset);
    }

    uintptr_t size() {
        return (*(uintptr_t*) at(_region_size_offset)) * sizeof(uintptr_t);
    }
};

class JVMFlag : VMStructs {
  public:
    static JVMFlag* find(const char* name);

    const char* name() {
        return *(const char**) at(_flag_name_offset);
    }

    char* addr() {
        return *(char**) at(_flag_addr_offset);
    }

    char origin() {
        return _flag_origin_offset >= 0 ? (*(char*) at(_flag_origin_offset)) & 15 : 0;
    }

    char get() {
        return *addr();
    }

    void set(char value) {
        *addr() = value;
    }
};

class PcDesc {
  public:
    int _pc;
    int _scope_offset;
    int _obj_offset;
    int _flags;
};

class ScopeDesc : VMStructs {
  private:
    const unsigned char* _scopes;
    VMMethod** _metadata;
    const unsigned char* _stream;
    int _method_offset;
    int _bci;

    int readInt();

  public:
    ScopeDesc(NMethod* nm) {
        _scopes = (const unsigned char*)nm->scopes();
        _metadata = nm->metadata();
    }

    int decode(int offset) {
        _stream = _scopes + offset;
        int sender_offset = readInt();
        _method_offset = readInt();
        _bci = readInt() - 1;
        return sender_offset;
    }

    VMMethod* method() {
        return _method_offset > 0 ? _metadata[_method_offset - 1] : NULL;
    }

    int bci() {
        return _bci;
    }
};

class InterpreterFrame : VMStructs {
  public:
    enum {
        sender_sp_offset = -1,
        method_offset = -3
    };

    static int bcp_offset() {
        return _interpreter_frame_bcp_offset;
    }
};

#endif // _VMSTRUCTS_H
