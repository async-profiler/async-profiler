/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <unistd.h>
#include "vmStructs.h"
#include "vmEntry.h"
#include "j9Ext.h"
#include "safeAccess.h"


CodeCache* VMStructs::_libjvm = NULL;

bool VMStructs::_has_class_names = false;
bool VMStructs::_has_method_structs = false;
bool VMStructs::_has_compiler_structs = false;
bool VMStructs::_has_stack_structs = false;
bool VMStructs::_has_class_loader_data = false;
bool VMStructs::_has_native_thread_id = false;
bool VMStructs::_has_perm_gen = false;
bool VMStructs::_compact_object_headers = false;

int VMStructs::_klass_name_offset = -1;
int VMStructs::_symbol_length_offset = -1;
int VMStructs::_symbol_length_and_refcount_offset = -1;
int VMStructs::_symbol_body_offset = -1;
int VMStructs::_oop_klass_offset = -1;
int VMStructs::_class_loader_data_offset = -1;
int VMStructs::_class_loader_data_next_offset = -1;
int VMStructs::_methods_offset = -1;
int VMStructs::_jmethod_ids_offset = -1;
int VMStructs::_thread_osthread_offset = -1;
int VMStructs::_thread_anchor_offset = -1;
int VMStructs::_thread_state_offset = -1;
int VMStructs::_thread_vframe_offset = -1;
int VMStructs::_thread_exception_offset = -1;
int VMStructs::_comp_env_offset = -1;
int VMStructs::_comp_task_offset = -1;
int VMStructs::_comp_method_offset = -1;
int VMStructs::_osthread_id_offset = -1;
int VMStructs::_anchor_sp_offset = -1;
int VMStructs::_anchor_pc_offset = -1;
int VMStructs::_anchor_fp_offset = -1;
int VMStructs::_frame_size_offset = -1;
int VMStructs::_frame_complete_offset = -1;
int VMStructs::_code_offset = -1;
int VMStructs::_data_offset = -1;
int VMStructs::_scopes_pcs_offset = -1;
int VMStructs::_scopes_data_offset = -1;
int VMStructs::_nmethod_name_offset = -1;
int VMStructs::_nmethod_method_offset = -1;
int VMStructs::_nmethod_entry_offset = -1;
int VMStructs::_nmethod_state_offset = -1;
int VMStructs::_nmethod_level_offset = -1;
int VMStructs::_nmethod_metadata_offset = -1;
int VMStructs::_nmethod_immutable_offset = -1;
int VMStructs::_method_constmethod_offset = -1;
int VMStructs::_method_code_offset = -1;
int VMStructs::_constmethod_constants_offset = -1;
int VMStructs::_constmethod_idnum_offset = -1;
int VMStructs::_constmethod_size = -1;
int VMStructs::_pool_holder_offset = -1;
int VMStructs::_array_len_offset = 0;
int VMStructs::_array_data_offset = -1;
int VMStructs::_code_heap_memory_offset = -1;
int VMStructs::_code_heap_segmap_offset = -1;
int VMStructs::_code_heap_segment_shift = -1;
int VMStructs::_heap_block_used_offset = -1;
int VMStructs::_vs_low_bound_offset = -1;
int VMStructs::_vs_high_bound_offset = -1;
int VMStructs::_vs_low_offset = -1;
int VMStructs::_vs_high_offset = -1;
int VMStructs::_flag_name_offset = -1;
int VMStructs::_flag_addr_offset = -1;
int VMStructs::_flag_origin_offset = -1;
const char* VMStructs::_flags_addr = NULL;
int VMStructs::_flag_count = 0;
int VMStructs::_flag_size = 0;
char* VMStructs::_code_heap[3] = {};
const void* VMStructs::_code_heap_low = NO_MIN_ADDRESS;
const void* VMStructs::_code_heap_high = NO_MAX_ADDRESS;
char** VMStructs::_code_heap_addr = NULL;
const void** VMStructs::_code_heap_low_addr = NULL;
const void** VMStructs::_code_heap_high_addr = NULL;
int* VMStructs::_klass_offset_addr = NULL;
char** VMStructs::_narrow_klass_base_addr = NULL;
char* VMStructs::_narrow_klass_base = NULL;
int* VMStructs::_narrow_klass_shift_addr = NULL;
int VMStructs::_narrow_klass_shift = -1;
char** VMStructs::_collected_heap_addr = NULL;
char* VMStructs::_collected_heap = NULL;
int VMStructs::_collected_heap_reserved_offset = -1;
int VMStructs::_region_start_offset = -1;
int VMStructs::_region_size_offset = -1;
int VMStructs::_markword_klass_shift = -1;
int VMStructs::_markword_monitor_value = -1;
int VMStructs::_interpreter_frame_bcp_offset = 0;
unsigned char VMStructs::_unsigned5_base = 0;
const void** VMStructs::_call_stub_return_addr = NULL;
const void* VMStructs::_call_stub_return = NULL;
const void* VMStructs::_interpreted_frame_valid_start = NULL;
const void* VMStructs::_interpreted_frame_valid_end = NULL;

jfieldID VMStructs::_eetop;
jfieldID VMStructs::_tid;
jfieldID VMStructs::_klass = NULL;
int VMStructs::_tls_index = -1;
intptr_t VMStructs::_env_offset;

VMStructs::GetStackTraceFunc VMStructs::_get_stack_trace = NULL;
VMStructs::LockFunc VMStructs::_lock_func;
VMStructs::LockFunc VMStructs::_unlock_func;


uintptr_t VMStructs::readSymbol(const char* symbol_name) {
    const void* symbol = _libjvm->findSymbol(symbol_name);
    if (symbol == NULL) {
        // Avoid JVM crash in case of missing symbols
        return 0;
    }
    return *(uintptr_t*)symbol;
}

// Run at agent load time
void VMStructs::init(CodeCache* libjvm) {
    _libjvm = libjvm;

    if (!VM::isOpenJ9() && !VM::isZing()) {
        initOffsets();
        initJvmFunctions();
    }
}

// Run when VM is initialized and JNI is available
void VMStructs::ready() {
    resolveOffsets();
    patchSafeFetch();
    initThreadBridge();
}

void VMStructs::initOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    uintptr_t offset_offset = readSymbol("gHotSpotVMStructEntryOffsetOffset");
    uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *(const char**)(entry + type_offset);
            const char* field = *(const char**)(entry + field_offset);
            if (type == NULL || field == NULL) {
                break;
            }

            if (strcmp(type, "Klass") == 0) {
                if (strcmp(field, "_name") == 0) {
                    _klass_name_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "Symbol") == 0) {
                if (strcmp(field, "_length") == 0) {
                    _symbol_length_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_length_and_refcount") == 0) {
                    _symbol_length_and_refcount_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_body") == 0) {
                    _symbol_body_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "oopDesc") == 0) {
                if (strcmp(field, "_metadata._klass") == 0) {
                    _oop_klass_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "Universe") == 0 || strcmp(type, "CompressedKlassPointers") == 0) {
                if (strcmp(field, "_narrow_klass._base") == 0 || strcmp(field, "_base") == 0) {
                    _narrow_klass_base_addr = *(char***)(entry + address_offset);
                } else if (strcmp(field, "_narrow_klass._shift") == 0 || strcmp(field, "_shift") == 0) {
                    _narrow_klass_shift_addr = *(int**)(entry + address_offset);
                } else if (strcmp(field, "_collectedHeap") == 0) {
                    _collected_heap_addr = *(char***)(entry + address_offset);
                }
            } else if (strcmp(type, "CollectedHeap") == 0) {
                if (strcmp(field, "_reserved") == 0) {
                    _collected_heap_reserved_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "MemRegion") == 0) {
                if (strcmp(field, "_start") == 0) {
                    _region_start_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_word_size") == 0) {
                    _region_size_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "CompiledMethod") == 0 || strcmp(type, "nmethod") == 0) {
                if (strcmp(field, "_method") == 0) {
                    _nmethod_method_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_verified_entry_offset") == 0) {
                    _nmethod_entry_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_verified_entry_point") == 0) {
                    _nmethod_entry_offset = - *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_state") == 0) {
                    _nmethod_state_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_comp_level") == 0) {
                    _nmethod_level_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_metadata_offset") == 0) {
                    _nmethod_metadata_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_immutable_data") == 0) {
                    _nmethod_immutable_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_scopes_pcs_offset") == 0) {
                    _scopes_pcs_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_scopes_data_offset") == 0) {
                    _scopes_data_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_scopes_data_begin") == 0) {
                    _scopes_data_offset = - *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "Method") == 0) {
                if (strcmp(field, "_constMethod") == 0) {
                    _method_constmethod_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_code") == 0) {
                    _method_code_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "ConstMethod") == 0) {
                if (strcmp(field, "_constants") == 0) {
                    _constmethod_constants_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_method_idnum") == 0) {
                    _constmethod_idnum_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "ConstantPool") == 0) {
                if (strcmp(field, "_pool_holder") == 0) {
                    _pool_holder_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "InstanceKlass") == 0) {
                if (strcmp(field, "_class_loader_data") == 0) {
                    _class_loader_data_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_methods") == 0) {
                    _methods_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_methods_jmethod_ids") == 0) {
                    _jmethod_ids_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "ClassLoaderData") == 0) {
                if (strcmp(field, "_next") == 0) {
                    _class_loader_data_next_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "java_lang_Class") == 0) {
                if (strcmp(field, "_klass_offset") == 0) {
                    _klass_offset_addr = *(int**)(entry + address_offset);
                }
            } else if (strcmp(type, "JavaThread") == 0) {
                if (strcmp(field, "_osthread") == 0) {
                    _thread_osthread_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_anchor") == 0) {
                    _thread_anchor_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_thread_state") == 0) {
                    _thread_state_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_vframe_array_head") == 0) {
                    _thread_vframe_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "ThreadShadow") == 0) {
                if (strcmp(field, "_exception_file") == 0) {
                    _thread_exception_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "OSThread") == 0) {
                if (strcmp(field, "_thread_id") == 0) {
                    _osthread_id_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "CompilerThread") == 0) {
                if (strcmp(field, "_env") == 0) {
                    _comp_env_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "ciEnv") == 0) {
                if (strcmp(field, "_task") == 0) {
                    _comp_task_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "CompileTask") == 0) {
                if (strcmp(field, "_method") == 0) {
                    _comp_method_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "JavaFrameAnchor") == 0) {
                if (strcmp(field, "_last_Java_sp") == 0) {
                    _anchor_sp_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_last_Java_pc") == 0) {
                    _anchor_pc_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_last_Java_fp") == 0) {
                    _anchor_fp_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "CodeBlob") == 0) {
                if (strcmp(field, "_frame_size") == 0) {
                    _frame_size_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_frame_complete_offset") == 0) {
                    _frame_complete_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_code_offset") == 0) {
                    _code_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_code_begin") == 0) {
                    _code_offset = - *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_data_offset") == 0) {
                    _data_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_name") == 0) {
                    _nmethod_name_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "CodeCache") == 0) {
                if (strcmp(field, "_heap") == 0) {
                    _code_heap_addr = *(char***)(entry + address_offset);
                } else if (strcmp(field, "_heaps") == 0) {
                    _code_heap_addr = *(char***)(entry + address_offset);
                } else if (strcmp(field, "_low_bound") == 0) {
                    _code_heap_low_addr = *(const void***)(entry + address_offset);
                } else if (strcmp(field, "_high_bound") == 0) {
                    _code_heap_high_addr = *(const void***)(entry + address_offset);
                }
            } else if (strcmp(type, "CodeHeap") == 0) {
                if (strcmp(field, "_memory") == 0) {
                    _code_heap_memory_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_segmap") == 0) {
                    _code_heap_segmap_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_log2_segment_size") == 0) {
                    _code_heap_segment_shift = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "HeapBlock::Header") == 0) {
                if (strcmp(field, "_used") == 0) {
                    _heap_block_used_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "VirtualSpace") == 0) {
                if (strcmp(field, "_low_boundary") == 0) {
                    _vs_low_bound_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_high_boundary") == 0) {
                    _vs_high_bound_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_low") == 0) {
                    _vs_low_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_high") == 0) {
                    _vs_high_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "StubRoutines") == 0) {
                if (strcmp(field, "_call_stub_return_address") == 0) {
                    _call_stub_return_addr = *(const void***)(entry + address_offset);
                }
            } else if (strcmp(type, "GrowableArrayBase") == 0 || strcmp(type, "GenericGrowableArray") == 0) {
                if (strcmp(field, "_len") == 0) {
                    _array_len_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "GrowableArray<int>") == 0) {
                if (strcmp(field, "_data") == 0) {
                    _array_data_offset = *(int*)(entry + offset_offset);
                }
            } else if (strcmp(type, "JVMFlag") == 0 || strcmp(type, "Flag") == 0) {
                if (strcmp(field, "_name") == 0 || strcmp(field, "name") == 0) {
                    _flag_name_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_addr") == 0 || strcmp(field, "addr") == 0) {
                    _flag_addr_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "_flags") == 0 || strcmp(field, "origin") == 0) {
                    _flag_origin_offset = *(int*)(entry + offset_offset);
                } else if (strcmp(field, "flags") == 0) {
                    _flags_addr = **(char***)(entry + address_offset);
                } else if (strcmp(field, "numFlags") == 0) {
                    _flag_count = **(int**)(entry + address_offset);
                }
            } else if (strcmp(type, "PcDesc") == 0) {
                // TODO
            } else if (strcmp(type, "PermGen") == 0) {
                _has_perm_gen = true;
            }
        }
    }

    entry = readSymbol("gHotSpotVMTypes");
    stride = readSymbol("gHotSpotVMTypeEntryArrayStride");
    type_offset = readSymbol("gHotSpotVMTypeEntryTypeNameOffset");
    uintptr_t size_offset = readSymbol("gHotSpotVMTypeEntrySizeOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* type = *(const char**)(entry + type_offset);
            if (type == NULL) {
                break;
            }

            if (strcmp(type, "JVMFlag") == 0 || strcmp(type, "Flag") == 0) {
                _flag_size = *(int*)(entry + size_offset);
            } else if (strcmp(type, "ConstMethod") == 0) {
                _constmethod_size = *(int*)(entry + size_offset);
            }
        }
    }

    entry = readSymbol("gHotSpotVMLongConstants");
    stride = readSymbol("gHotSpotVMLongConstantEntryArrayStride");
    uintptr_t name_offset = readSymbol("gHotSpotVMLongConstantEntryNameOffset");
    uintptr_t value_offset = readSymbol("gHotSpotVMLongConstantEntryValueOffset");

    if (entry != 0 && stride != 0) {
        for (;; entry += stride) {
            const char* name = *(const char**)(entry + name_offset);
            if (name == NULL) {
                break;
            }

            if (strncmp(name, "markWord::", 10) == 0) {
                if (strcmp(name + 10, "klass_shift") == 0) {
                    _markword_klass_shift = *(long*)(entry + value_offset);
                } else if (strcmp(name + 10, "monitor_value") == 0) {
                    _markword_monitor_value = *(long*)(entry + value_offset);
                }
            }
        }
    }
}

void VMStructs::resolveOffsets() {
    if (_klass_offset_addr != NULL) {
        _klass = (jfieldID)(uintptr_t)(*_klass_offset_addr << 2 | 2);
    }

    JVMFlag* ccp = JVMFlag::find("UseCompressedClassPointers");
    if (ccp != NULL && ccp->get() && _narrow_klass_base_addr != NULL && _narrow_klass_shift_addr != NULL) {
        _narrow_klass_base = *_narrow_klass_base_addr;
        _narrow_klass_shift = *_narrow_klass_shift_addr;
    }

    JVMFlag* coh = JVMFlag::find("UseCompactObjectHeaders");
    if (coh != NULL && coh->get()) {
        _compact_object_headers = true;
    }

    _has_class_names = _klass_name_offset >= 0
            && (_compact_object_headers ? (_markword_klass_shift >= 0 && _markword_monitor_value == MONITOR_BIT)
                                        : _oop_klass_offset >= 0)
            && (_symbol_length_offset >= 0 || _symbol_length_and_refcount_offset >= 0)
            && _symbol_body_offset >= 0
            && _klass != NULL;

    _has_method_structs = _jmethod_ids_offset >= 0
            && _nmethod_method_offset >= 0
            && _nmethod_entry_offset != -1
            && _nmethod_state_offset >= 0
            && _method_constmethod_offset >= 0
            && _method_code_offset >= 0
            && _constmethod_constants_offset >= 0
            && _constmethod_idnum_offset >= 0
            && _constmethod_size >= 0
            && _pool_holder_offset >= 0;

    _has_compiler_structs = _comp_env_offset >= 0
            && _comp_task_offset >= 0
            && _comp_method_offset >= 0;

    _has_class_loader_data = _class_loader_data_offset >= 0
            && _class_loader_data_next_offset == sizeof(uintptr_t) * 8 + 8
            && _methods_offset >= 0
            && _klass != NULL
            && _lock_func != NULL && _unlock_func != NULL;

#if defined(__x86_64__)
    _interpreter_frame_bcp_offset = VM::hotspot_version() >= 11 ? -8 : VM::hotspot_version() == 8 ? -7 : 0;
#elif defined(__aarch64__)
    _interpreter_frame_bcp_offset = VM::hotspot_version() >= 11 ? -9 : VM::hotspot_version() == 8 ? -7 : 0;
#endif

    // JDK-8292758 has slightly changed ScopeDesc encoding
    if (VM::hotspot_version() >= 20) {
        _unsigned5_base = 1;
    }

    if (_call_stub_return_addr != NULL) {
        _call_stub_return = *_call_stub_return_addr;
    }

    // Since JDK 23, _metadata_offset is relative to _data_offset. See metadata()
    if (_nmethod_immutable_offset < 0) {
        _data_offset = 0;
    }

    _has_stack_structs = _has_method_structs
            && _interpreter_frame_bcp_offset != 0
            && _code_offset != -1
            && _data_offset >= 0
            && _scopes_data_offset != -1
            && _scopes_pcs_offset >= 0
            && _nmethod_metadata_offset >= 0
            && _thread_vframe_offset >= 0
            && _thread_exception_offset >= 0
            && _constmethod_size >= 0;

    if (_code_heap_addr != NULL && _code_heap_low_addr != NULL && _code_heap_high_addr != NULL) {
        char* code_heaps = *_code_heap_addr;
        unsigned int code_heap_count = *(unsigned int*)(code_heaps + _array_len_offset);
        if (code_heap_count <= 3 && _array_data_offset >= 0) {
            char* code_heap_array = *(char**)(code_heaps + _array_data_offset);
            memcpy(_code_heap, code_heap_array, code_heap_count * sizeof(_code_heap[0]));
        }
        _code_heap_low = *_code_heap_low_addr;
        _code_heap_high = *_code_heap_high_addr;
    } else if (_code_heap_addr != NULL && _code_heap_memory_offset >= 0) {
        _code_heap[0] = *_code_heap_addr;
        _code_heap_low = *(const void**)(_code_heap[0] + _code_heap_memory_offset + _vs_low_bound_offset);
        _code_heap_high = *(const void**)(_code_heap[0] + _code_heap_memory_offset + _vs_high_bound_offset);
    }

    // Invariant: _code_heap[i] != NULL iff all CodeHeap structures are available
    if (_code_heap[0] != NULL && _code_heap_segment_shift >= 0) {
        _code_heap_segment_shift = *(int*)(_code_heap[0] + _code_heap_segment_shift);
    }
    if (_code_heap_memory_offset < 0 || _code_heap_segmap_offset < 0 ||
        _code_heap_segment_shift < 0 || _code_heap_segment_shift > 16 ||
        _heap_block_used_offset < 0) {
        memset(_code_heap, 0, sizeof(_code_heap));
    }

    if (_collected_heap_addr != NULL && _collected_heap_reserved_offset >= 0 &&
        _region_start_offset >= 0 && _region_size_offset >= 0) {
        _collected_heap = *_collected_heap_addr + _collected_heap_reserved_offset;
    }
}

void VMStructs::initJvmFunctions() {
    _get_stack_trace = (GetStackTraceFunc)_libjvm->findSymbolByPrefix("_ZN8JvmtiEnv13GetStackTraceEP10JavaThreadiiP");

    if (VM::hotspot_version() == 8) {
        _lock_func = (LockFunc)_libjvm->findSymbol("_ZN7Monitor28lock_without_safepoint_checkEv");
        _unlock_func = (LockFunc)_libjvm->findSymbol("_ZN7Monitor6unlockEv");
    }

    if (VM::hotspot_version() > 0) {
        CodeBlob* blob = _libjvm->findBlob("_ZNK5frame26is_interpreted_frame_validEP10JavaThread");
        if (blob != NULL) {
            _interpreted_frame_valid_start = blob->_start;
            _interpreted_frame_valid_end = blob->_end;
        }
    }
}

void VMStructs::patchSafeFetch() {
    // Workarounds for JDK-8307549 and JDK-8321116
    if (WX_MEMORY && VM::hotspot_version() == 17) {
        void** entry = (void**)_libjvm->findSymbol("_ZN12StubRoutines18_safefetch32_entryE");
        if (entry != NULL) {
            *entry = (void*)SafeAccess::load32;
        }
    } else if (WX_MEMORY && VM::hotspot_version() == 11) {
        void** entry = (void**)_libjvm->findSymbol("_ZN12StubRoutines17_safefetchN_entryE");
        if (entry != NULL) {
            *entry = (void*)SafeAccess::loadPtr;
        }
    }
}

void VMStructs::initTLS(void* vm_thread) {
    for (int i = 0; i < 1024; i++) {
        if (pthread_getspecific((pthread_key_t)i) == vm_thread) {
            _tls_index = i;
            break;
        }
    }
}

void VMStructs::initThreadBridge() {
    jthread thread;
    if (VM::jvmti()->GetCurrentThread(&thread) != 0) {
        return;
    }

    JNIEnv* env = VM::jni();

    // Get eetop field - a bridge from Java Thread to VMThread
    jclass thread_class = env->FindClass("java/lang/Thread");
    if (thread_class == NULL ||
        (_tid = env->GetFieldID(thread_class, "tid", "J")) == NULL ||
        (_eetop = env->GetFieldID(thread_class, "eetop", "J")) == NULL) {
        // No such field - probably not a HotSpot JVM
        env->ExceptionClear();

        void* j9thread = J9Ext::j9thread_self();
        if (j9thread != NULL) {
            initTLS(j9thread);
        }
    } else {
        // HotSpot
        VMThread* vm_thread = VMThread::fromJavaThread(env, thread);
        if (vm_thread != NULL) {
            _env_offset = (intptr_t)env - (intptr_t)vm_thread;
            _has_native_thread_id = _thread_osthread_offset >= 0 && _osthread_id_offset >= 0;
            initTLS(vm_thread);
        }
    }
}

VMThread* VMThread::current() {
    return _tls_index >= 0 ? (VMThread*)pthread_getspecific((pthread_key_t)_tls_index) : NULL;
}

int VMThread::nativeThreadId(JNIEnv* jni, jthread thread) {
    if (_has_native_thread_id) {
        VMThread* vm_thread = fromJavaThread(jni, thread);
        return vm_thread != NULL ? vm_thread->osThreadId() : -1;
    }
    return VM::isOpenJ9() ? J9Ext::GetOSThreadID(thread) : -1;
}

jmethodID VMMethod::id() {
    // We may find a bogus NMethod during stack walking, it does not always point to a valid VMMethod
    const char* const_method = (const char*) SafeAccess::load((void**) at(_method_constmethod_offset));
    if (!goodPtr(const_method)) {
        return NULL;
    }

    const char* cpool = *(const char**) (const_method + _constmethod_constants_offset);
    unsigned short num = *(unsigned short*) (const_method + _constmethod_idnum_offset);
    if (goodPtr(cpool)) {
        VMKlass* holder = *(VMKlass**)(cpool + _pool_holder_offset);
        if (goodPtr(holder)) {
            jmethodID* ids = holder->jmethodIDs();
            if (ids != NULL && num < (size_t)ids[0]) {
                return ids[num + 1];
            }
        }
    }
    return NULL;
}

NMethod* CodeHeap::findNMethod(char* heap, const void* pc) {
    unsigned char* heap_start = *(unsigned char**)(heap + _code_heap_memory_offset + _vs_low_offset);
    unsigned char* segmap = *(unsigned char**)(heap + _code_heap_segmap_offset + _vs_low_offset);
    size_t idx = ((unsigned char*)pc - heap_start) >> _code_heap_segment_shift;

    if (segmap[idx] == 0xff) {
        return NULL;
    }
    while (segmap[idx] > 0) {
        idx -= segmap[idx];
    }

    unsigned char* block = heap_start + (idx << _code_heap_segment_shift) + _heap_block_used_offset;
    return *block ? align<NMethod*>(block + sizeof(uintptr_t)) : NULL;
}

JVMFlag* JVMFlag::find(const char* name) {
    if (_flags_addr != NULL && _flag_size > 0) {
        for (int i = 0; i < _flag_count; i++) {
            JVMFlag* f = (JVMFlag*)(_flags_addr + i * _flag_size);
            if (f->name() != NULL && strcmp(f->name(), name) == 0 && f->addr() != NULL) {
                return f;
            }
        }
    }
    return NULL;
}

int NMethod::findScopeOffset(const void* pc) {
    intptr_t pc_offset = (const char*)pc - code();
    if (pc_offset < 0 || pc_offset > 0x7fffffff) {
        return -1;
    }

    const int* scopes_pcs = (const int*) at(_scopes_pcs_offset);
    PcDesc* pcd = (PcDesc*) immutableDataAt(scopes_pcs[0]);
    PcDesc* pcd_end = (PcDesc*) immutableDataAt(scopes_pcs[1]);
    int low = 0;
    int high = (pcd_end - pcd) - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (pcd[mid]._pc < pc_offset) {
            low = mid + 1;
        } else if (pcd[mid]._pc > pc_offset) {
            high = mid - 1;
        } else {
            return pcd[mid]._scope_offset;
        }
    }

    return pcd + low < pcd_end ? pcd[low]._scope_offset : -1;
}

int ScopeDesc::readInt() {
    unsigned char c = *_stream++;
    unsigned int n = c - _unsigned5_base;
    if (c >= 192) {
        for (int shift = 6; ; shift += 6) {
            c = *_stream++;
            n += (c - _unsigned5_base) << shift;
            if (c < 192 || shift >= 24) break;
        }
    }
    return n;
}
