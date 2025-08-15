/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <limits>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "classfile_constants.h"
#include "incbin.h"
#include "log.h"
#include "os.h"
#include "profiler.h"
#include "tsc.h"
#include "vmEntry.h"
#include "instrument.h"

constexpr u32 MAX_CODE_SEGMENT_BYTES = 65534;
constexpr int16_t INT16_T_MAX_VALUE = 0x7fff;

INCLUDE_HELPER_CLASS(INSTRUMENT_NAME, INSTRUMENT_CLASS, "one/profiler/Instrument")

u8 countParametersSlots(const char* method_sig) {
    u8 count = 0;
    size_t i = 1;
    while (method_sig[i] != ')') {
        if (method_sig[i] == 'L') {
            count += 1;
            while (method_sig[++i] != ';');
        } else if (method_sig[i] == '[') {
            ++count;
            while (method_sig[++i] == '[');
            if (method_sig[i] == 'L') while (method_sig[++i] != ';');
        } else if (method_sig[i] == 'J' || method_sig[i] == 'D') {
            count += 2;
        } else {
            ++count;
        }
        ++i;
    }
    return count;
}

inline u32 smallestGreaterMultiple4(u32 i) {
    return (i / 4) * 4 + 4;
}

enum ConstantTag {
    CONSTANT_Utf8 = 1,
    CONSTANT_Integer = 3,
    CONSTANT_Float = 4,
    CONSTANT_Long = 5,
    CONSTANT_Double = 6,
    CONSTANT_Class = 7,
    CONSTANT_String = 8,
    CONSTANT_Fieldref = 9,
    CONSTANT_Methodref = 10,
    CONSTANT_InterfaceMethodref = 11,
    CONSTANT_NameAndType = 12,
    CONSTANT_MethodHandle = 15,
    CONSTANT_MethodType = 16,
    CONSTANT_Dynamic = 17,
    CONSTANT_InvokeDynamic = 18,
    CONSTANT_Module = 19,
    CONSTANT_Package = 20
};

class Constant {
  private:
    u8 _tag;
    u8 _info[2];

  public:
    u8 tag() {
        return _tag;
    }

    int slots() {
        return _tag == CONSTANT_Long || _tag == CONSTANT_Double ? 2 : 1;
    }

    u16 info() {
        return (u16)_info[0] << 8 | (u16)_info[1];
    }

    int length() {
        switch (_tag) {
            case CONSTANT_Utf8:
                return 2 + info();
            case CONSTANT_Integer:
            case CONSTANT_Float:
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref:
            case CONSTANT_NameAndType:
            case CONSTANT_Dynamic:
            case CONSTANT_InvokeDynamic:
                return 4;
            case CONSTANT_Long:
            case CONSTANT_Double:
                return 8;
            case CONSTANT_Class:
            case CONSTANT_String:
            case CONSTANT_MethodType:
            case CONSTANT_Module:
            case CONSTANT_Package:
                return 2;
            case CONSTANT_MethodHandle:
                return 3;
            default:
                return 0;
        }
    }

    bool equals(const char* value, u16 len) {
        return _tag == CONSTANT_Utf8 && info() == len && memcmp(_info + 2, value, len) == 0;
    }

    bool matches(const char* value, u16 len) {
        if (len > 0 && value[len - 1] == '*') {
            return _tag == CONSTANT_Utf8 && info() >= len - 1 && memcmp(_info + 2, value, len - 1) == 0;
        }
        return equals(value, len);
    }

    u8 getCountParametersSlots() const {
        return countParametersSlots((const char*) (_info + 2));
    }
};

enum Scope {
    SCOPE_CLASS,
    SCOPE_FIELD,
    SCOPE_METHOD,
    SCOPE_REWRITE_METHOD
};

enum PatchConstants {
    EXTRA_CONSTANTS = 16,
    EXTRA_BYTECODES = 4
};


class BytecodeRewriter {
  private:
    const u8* _src;
    const u8* _src_limit;

    u8* _dst;
    u32 _dst_len;
    u32 _dst_capacity;

    Constant** _cpool;
    u16 _cpool_len;

    const char* _target_class;
    u16 _target_class_len;
    const char* _target_method;
    u16 _target_method_len;
    const char* _target_signature;
    u16 _target_signature_len;

    bool _latency_profiling = false;

    // Reader

    const u8* get(int bytes) {
        const u8* result = _src;
        _src += bytes;
        return _src <= _src_limit ? result : NULL;
    }

    u8 get8() {
        return *get(1);
    }

    u16 get16() {
        return ntohs(*(u16*)get(2));
    }

    u32 get32() {
        return ntohl(*(u32*)get(4));
    }

    u64 get64() {
        return OS::ntoh64(*(u64*)get(8));
    }

    Constant* getConstant() {
        Constant* c = (Constant*)get(1);
        get(c->length());
        return c;
    }

    // Writer

    u8* alloc(int bytes) {
        if (_dst_len + bytes > _dst_capacity) {
            grow(_dst_len + bytes + 2000);
        }
        u8* result = _dst + _dst_len;
        _dst_len += bytes;
        return result;
    }

    void grow(int new_capacity) {
        u8* new_dst = NULL;
        VM::jvmti()->Allocate(new_capacity, &new_dst);
        memcpy(new_dst, _dst, _dst_len);
        VM::jvmti()->Deallocate(_dst);

        _dst = new_dst;
        _dst_capacity = new_capacity;
    }

    void put(const u8* src, int bytes) {
        memcpy(alloc(bytes), src, bytes);
    }

    void put8(u8 v) {
        *alloc(1) = v;
    }

    void put16(u16 v) {
        *(u16*)alloc(2) = htons(v);
    }

    void put32(u32 v) {
        *(u32*)alloc(4) = htonl(v);
    }

    void put64(u64 v) {
        *(u64*)alloc(8) = OS::hton64(v);
    }

    void putConstant(const char* value) {
        u16 len = strlen(value);
        put8(CONSTANT_Utf8);
        put16(len);
        put((const u8*)value, len);
    }

    void putConstant(u8 tag, u16 ref) {
        put8(tag);
        put16(ref);
    }

    void putConstant(u8 tag, u16 ref1, u16 ref2) {
        put8(tag);
        put16(ref1);
        put16(ref2);
    }

    // BytecodeRewriter

    void rewriteCode(u16 access_flags, u16 descriptor_index);
    u32 rewriteCodeForLatency(const u8* code, u32 code_length, u8 start_time_loc_index, u32* relocation_table);
    void rewriteLineNumberTable(const u32* relocation_table);
    void rewriteLocalVariableTable(const u32* relocation_table, int new_local_index);
    void rewriteStackMapTable(const u32* relocation_table, u8 new_local_index);
    void rewriteVerificationTypeInfo(const u32* relocation_table);
    void rewriteAttributes(Scope scope, u16 access_flags = 0, u16 descriptor_index = 0);
    void rewriteCodeAttributes(const u32* relocation_table, int new_local_index);
    void rewriteMembers(Scope scope);
    bool rewriteClass();

  public:
    BytecodeRewriter(const u8* class_data, int class_data_len, const char* target_class, bool latency_profiling) :
        _src(class_data),
        _src_limit(class_data + class_data_len),
        _dst(NULL),
        _dst_len(0),
        _dst_capacity(class_data_len + 400),
        _cpool(NULL),
        _latency_profiling(latency_profiling) {

        _target_class = target_class;
        _target_class_len = strlen(_target_class);

        _target_method = _target_class + _target_class_len + 1;
        _target_signature = strchr(_target_method, '(');

        if (_target_signature == NULL) {
            _target_method_len = strlen(_target_method);
        } else {
            _target_method_len = _target_signature - _target_method;
            _target_signature_len = strlen(_target_signature);
        }
    }

    ~BytecodeRewriter() {
        delete[] _cpool;
    }

    void rewrite(u8** new_class_data, int* new_class_data_len) {
        if (VM::jvmti()->Allocate(_dst_capacity, &_dst) == 0) {
            if (rewriteClass()) {
                *new_class_data = _dst;
                *new_class_data_len = _dst_len;
            } else {
                VM::jvmti()->Deallocate(_dst);
            }
        }
    }
};

static inline bool isFunctionExit(u8 opcode) {
    return opcode >= JVM_OPC_ireturn && opcode <= JVM_OPC_return;
}

static inline bool isNarrowJump(u8 opcode) {
    return (opcode >= JVM_OPC_ifeq && opcode <= JVM_OPC_jsr) || opcode == JVM_OPC_ifnull || opcode == JVM_OPC_ifnonnull;
}

static inline bool isWideJump(u8 opcode) {
    return opcode == JVM_OPC_goto_w || opcode == JVM_OPC_jsr_w;
}

static inline u8 computeInstructionByteCount(const u8* code, u32 index) {
    static constexpr unsigned char OPCODE_LENGTH[JVM_OPC_MAX+1] = JVM_OPCODE_LENGTH_INITIALIZER;
    u8 opcode = code[index];
    if (opcode == JVM_OPC_wide) {
        if (code[index+1] == JVM_OPC_iinc) return 6;
        return 4;
    }
    if (opcode == JVM_OPC_tableswitch) {
        u32 default_index = smallestGreaterMultiple4(index);
        int32_t l = ntohl(*(u32*)(code + default_index + 4));
        int32_t h = ntohl(*(u32*)(code + default_index + 8));
        return default_index - index + (3 + (h - l + 1)) * 4;
    }
    if (opcode == JVM_OPC_lookupswitch) {
        u32 default_index = smallestGreaterMultiple4(index);
        u32 npairs = ntohl(*(u32*)(code + default_index + 4));
        return default_index - index + npairs * 8;
    }
    return OPCODE_LENGTH[opcode];
}


void BytecodeRewriter::rewriteCode(u16 access_flags, u16 descriptor_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    int code_begin = _dst_len;
    // Store the result of System.nanoTime()
    u8 new_variables = _latency_profiling ? 2 : 0;

    u16 max_stack = get16();
    put16(max_stack + new_variables);

    u16 max_locals = get16();
    put16(max_locals + new_variables);

    u32 code_length = get32();
    const u8* code = get(code_length);
    u32 code_length_idx = _dst_len;
    put32(code_length); // to be fixed later

    // For each index in the (original) bytecode, this holds the rightwards offset
    // in the modified bytecode.
    // This is code_length + 1 for convenience: sometimes we need to access the
    // code_length-ith index to refer to the first position after the code array
    // (i.e. LocalVariableTable).
    u32* relocation_table = new u32[code_length + 1];

    int new_local_index = -1;
    u32 relocation;
    if (_latency_profiling) {
        u8 parameters_count = _cpool[descriptor_index]->getCountParametersSlots();
        bool is_static = (access_flags & JVM_ACC_STATIC) != 0;
        new_local_index = parameters_count + (is_static ? 0 : 1);
        relocation = rewriteCodeForLatency(code, code_length, new_local_index, relocation_table);
    } else {
        // invokestatic "one/profiler/Instrument.recordEntry()V"
        put8(JVM_OPC_invokestatic);
        put16(_cpool_len);
        // nop ensures that tableswitch/lookupswitch needs no realignment
        put8(JVM_OPC_nop);
        relocation = EXTRA_BYTECODES;

        // The rest of the code is unchanged
        put(code, code_length);
        for (u32 i = 0; i <= code_length; ++i) relocation_table[i] = relocation;
    }

    // Fix code length, we now know the real relocation
    *(u32*)(_dst + code_length_idx) = htonl(code_length + relocation);

    u16 exception_table_length = get16();
    put16(exception_table_length);

    for (int i = 0; i < exception_table_length; i++) {
        u16 start_pc = get16();
        u16 end_pc = get16();
        u16 handler_pc = get16();
        u16 catch_type = get16();
        put16(start_pc + relocation_table[start_pc]);
        put16(end_pc + relocation_table[end_pc]);
        put16(handler_pc + relocation_table[handler_pc]);
        put16(catch_type);
    }

    rewriteCodeAttributes(relocation_table, new_local_index);
    delete[] relocation_table;

    // Patch attribute length
    *(u32*)(_dst + code_begin - 4) = htonl(_dst_len - code_begin);
}

// Return the relocation after the last byte of code
u32 BytecodeRewriter::rewriteCodeForLatency(const u8* code, u32 code_length, u8 start_time_loc_index, u32* relocation_table) {
    // Method start is relocated
    u32 current_relocation = EXTRA_BYTECODES * 2;

    // First scan: identify the maximum possible relocation for any position in code[]
    // Relocation can happen due to:
    // - Adding a new invocation
    // - Narrow jump becoming a wide jump
    u32 max_relocation = current_relocation;
    for (u32 i = 0; i < code_length;) {
        u8 opcode = code[i];
        if (isFunctionExit(opcode)) {
            max_relocation += EXTRA_BYTECODES;
        }
        i += computeInstructionByteCount(code, i);
    }

    if (max_relocation > MAX_CODE_SEGMENT_BYTES - code_length) {
        Log::warn("Instrumented code size exceeds JVM code segment size limit (%u), aborting instrumentation of %s.%s", MAX_CODE_SEGMENT_BYTES, _target_class, _target_method);
        put(code, code_length);
        for (u32 i = 0; i <= code_length; ++i) relocation_table[i] = 0;
        return 0;
    }

    u32 code_segment_begin = _dst_len;

    // invokestatic "java/lang/System.nanoTime()V"
    put8(JVM_OPC_invokestatic);
    put16(_cpool_len + 10);
    // nop ensures that tableswitch/lookupswitch needs no realignment
    put8(JVM_OPC_nop);

    put8(JVM_OPC_lstore);
    put8(start_time_loc_index);
    put16(JVM_OPC_nop);

    // Low 32 bits: jump base index
    // High 32 bits: jump offset index
    // This supports narrow and wide jumps, as well as tableswitch and lookupswitch
    std::vector<u64> jumps;
    jumps.reserve(code_length / 20);
    // Second scan: fill relocation_table and rewrite code.
    for (u32 i = 0; i < code_length;) {
        u8 opcode = code[i];

        if (isFunctionExit(opcode)) {
            put8(JVM_OPC_lload);
            put8(start_time_loc_index);
            put16(JVM_OPC_nop);

            // invokestatic "one/profiler/Instrument.recordExit(J)V"
            put8(JVM_OPC_invokestatic);
            put16(_cpool_len + 6);
            put8(JVM_OPC_nop);
        } else if (isNarrowJump(opcode)) {
            jumps.push_back((i + 1ULL) << 32 | i);
            int16_t offset = (int16_t) ntohs(*(u16*)(code + i + 1));
            if (max_relocation > INT16_T_MAX_VALUE - offset) {
                Log::warn("Narrow jump offset exceeds the limit for signed int16, aborting instrumentation of %s.", _target_class, _target_method);
                _dst_len = code_segment_begin;
                put(code, code_length);
                for (u32 i = 0; i < code_length; ++i) relocation_table[i] = 0;
                return 0;
            }
        } else if (isWideJump(opcode)) {
            jumps.push_back((i + 1ULL) << 32 | i);
        } else if (opcode == JVM_OPC_tableswitch) {
            // Nearest multiple of 4, 'default' lies after the padding
            u32 default_index = smallestGreaterMultiple4(i);
            // 4 bits: default
            jumps.push_back((u64) default_index << 32 | i);
            // 4 bits: low
            int32_t l = ntohl(*(u32*)(code + default_index + 4));
            // 4 bits: high
            int32_t h = ntohl(*(u32*)(code + default_index + 8));
            // (high - low + 1) * 4 bits: branches
            u32 branches_base_index = default_index + 12;
            for (u64 c = 0; c < h - l + 1; ++c) {
                jumps.push_back((branches_base_index + c * 4) << 32 | i);
            }
        } else if (opcode == JVM_OPC_lookupswitch) {
            // Nearest multiple of 4, 'default' lies after the padding
            u32 default_index = smallestGreaterMultiple4(i);
            // 4 bits: default
            jumps.push_back((u64) default_index << 32 | i);
            // 4 bits: npairs
            int32_t npairs = ntohl(*(u32*)(code + default_index + 4));
            u32 branches_base_index = default_index + 8;
            for (u64 c = 0; c < npairs; ++c) {
                u64 pair_base = branches_base_index + c * 8;
                // 4 bits: match
                // 4 bits: offset
                jumps.push_back((pair_base + 4) << 32 | i);
            }
        }
        u8 bc = computeInstructionByteCount(code, i);
        for (u32 args_idx = 0; args_idx < bc; ++args_idx) {
            relocation_table[i] = current_relocation;
            put8(code[i++]);
        }
        // current_relocation should be incremented for addresses after the
        // current instruction: any instruction referring to the current one
        // (e.g. a jump) should now target our invokestatic, otherwise it will
        // be skipped.
        if (isFunctionExit(opcode)) {
            current_relocation += EXTRA_BYTECODES * 2;
        } else if ((opcode >= JVM_OPC_iload && opcode <= JVM_OPC_aload) ||
                   (opcode >= JVM_OPC_istore && opcode <= JVM_OPC_astore)) {
            u8 index = code[i-1];
            if (index >= start_time_loc_index) {
                index += 2;
                *(_dst + _dst_len - 1) = index;
            }
        } else if ((opcode >= JVM_OPC_iload_0 && opcode <= JVM_OPC_aload_3) ||
                   (opcode >= JVM_OPC_istore_0 && opcode <= JVM_OPC_astore_3)) {
            u8 base = opcode <= JVM_OPC_aload_3 ? JVM_OPC_iload_0 : JVM_OPC_istore_0;
            u8 index = (opcode - base) % 4;
            if (index >= start_time_loc_index) {
                index += 2;
                if (index <= 3) {
                    *(_dst + _dst_len - 1) = opcode + 2;
                } else {
                    u8 new_opcode = (opcode <= JVM_OPC_aload_3 ? JVM_OPC_iload : JVM_OPC_istore) +
                                    (opcode - base) / 4;
                    *(_dst + _dst_len - 1) = new_opcode;
                    put8(index);
                    current_relocation += 1;
                }
            }
        } else if (opcode == JVM_OPC_iinc) {
            // TODO: handle wide
            u8 index = code[i-2];
            if (index >= start_time_loc_index) {
                // TODO: handle overflow here
                index += 2;
                *(_dst + _dst_len - 2) = index;
            }
        }
    }

    // Third scan (jumps only): fix the jump offset using information in relocation_table.
    for (u64 jump : jumps) {
        u32 old_jump_base_idx = (u32) jump;
        u32 old_jump_offset_idx = (u32) (jump >> 32);

        u32 new_jump_base_idx = old_jump_base_idx + relocation_table[old_jump_base_idx];
        u8* new_jump_base_ptr = _dst + code_segment_begin + new_jump_base_idx;
        u8* new_jump_offset_ptr = _dst + code_segment_begin + (old_jump_offset_idx + relocation_table[old_jump_offset_idx]);

        bool is_narrow = isNarrowJump(*new_jump_base_ptr);
        int32_t old_offset;
        if (is_narrow) {
            old_offset = (int32_t) (int16_t) ntohs(*(u16*)(code + old_jump_offset_idx));
        } else {
            old_offset = (int32_t) ntohl(*(u32*)(code + old_jump_offset_idx));
        }
        u32 old_jump_target = (u32) (old_jump_base_idx + old_offset);
        int32_t new_offset = old_jump_target + relocation_table[old_jump_target] - new_jump_base_idx;
        if (is_narrow) {
            *(u16*)(new_jump_offset_ptr) = htons((u16) new_offset);
        } else {
            *(u32*)(new_jump_offset_ptr) = htonl((u32) new_offset);
        }
    }

    relocation_table[code_length] = current_relocation;
    return current_relocation;
}

void BytecodeRewriter::rewriteLineNumberTable(const u32* relocation_table) {
    u32 attribute_length = get32();
    put32(attribute_length);

    u16 table_length = get16();
    put16(table_length);

    for (int i = 0; i < table_length; i++) {
        u16 start_pc = get16();
        put16(start_pc + relocation_table[start_pc]);
        put16(get16());
    }
}

void BytecodeRewriter::rewriteLocalVariableTable(const u32* relocation_table, int new_local_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    u16 table_length = get16();
    put16(table_length);

    for (int i = 0; i < table_length; i++) {
        u16 start_pc_old = get16();
        u16 start_pc_new = start_pc_old + relocation_table[start_pc_old];
        put16(start_pc_new);

        u16 length_old = get16();
        u16 end_pc_old = start_pc_old + length_old;
        u16 end_pc_new = end_pc_old + relocation_table[end_pc_old];
        put16(end_pc_new - start_pc_new);

        put32(get32());

        u16 index = get16();
        if (index >= new_local_index) {
            // The new variable is a long
            index += 2;
        }
        put16(index);
    }
}

// Return the new offset delta
u16 updateCurrentFrame(long& current_frame_old, long& current_frame_new, 
                       u16 offset_delta_old, const u32* relocation_table) {
    current_frame_old += offset_delta_old + 1;
    u64 previous_frame_new = current_frame_new;
    current_frame_new = current_frame_old + relocation_table[current_frame_old];
    return current_frame_new - previous_frame_new - 1;
}

void BytecodeRewriter::rewriteStackMapTable(const u32* relocation_table, u8 new_local_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    // Current instruction index in the old code
    long current_frame_old = -1;
    // And in the new code
    long current_frame_new = -1;

    u32 attribute_start_idx = _dst_len;
    u16 number_of_entries = get16();

    // Latency profiling may have bailed out
    bool latency_profiling_ok = _latency_profiling && relocation_table[0] > 0;
    if (latency_profiling_ok) {
        put16(number_of_entries + 1);
        // The new stackframe is applied to the nop just after the lstore,
        // so we don't need to worry about conflicts with existing frames
        // in the same spot.
        current_frame_new = 7;
        put8(252);
        put16(current_frame_new);
        put8(JVM_ITEM_Long);
    } else {
        put16(number_of_entries);
    }

    for (int i = 0; i < number_of_entries; i++) {
        u8 frame_type = get8();
        u32 stack_map_frame_idx = _dst_len;
        put8(frame_type);

        if (frame_type <= 127) {
            // same_frame and same_locals_1_stack_item_frame
            u16 new_offset_delta = updateCurrentFrame(current_frame_old, current_frame_new, 
                                                      frame_type % 64, relocation_table);

            u8 new_frame_type;
            if (new_offset_delta <= 63) {
                new_frame_type = new_offset_delta + (frame_type / 64 * 64);
            } else {
                // Convert to same_locals_1_stack_item_frame_extended or same_frame_extended
                new_frame_type = frame_type > 63 ? 247 : 251;
                put16(new_offset_delta);
            }
            // Patch frame type
            *(_dst + stack_map_frame_idx) = new_frame_type;
            if (frame_type > 63) {
                rewriteVerificationTypeInfo(relocation_table);
            }
        } else if (frame_type == 247) {
            // same_locals_1_stack_item_frame_extended
            put16(updateCurrentFrame(current_frame_old, current_frame_new, get16(), relocation_table));
            rewriteVerificationTypeInfo(relocation_table);
        } else if (frame_type <= 251) {
            // chop_frame or same_frame_extended
            put16(updateCurrentFrame(current_frame_old, current_frame_new, get16(), relocation_table));
        } else if (frame_type <= 254) {
            // append_frame
            put16(updateCurrentFrame(current_frame_old, current_frame_new, get16(), relocation_table));
            u8 count = frame_type - (u8)251; // explicit cast to workaround clang type promotion bug
            for (u8 j = 0; j < count; j++) {
                rewriteVerificationTypeInfo(relocation_table);
            }
        } else {
            // full_frame
            put16(updateCurrentFrame(current_frame_old, current_frame_new, get16(), relocation_table));
            u16 number_of_locals = get16() + latency_profiling_ok;
            put16(number_of_locals);
            for (int j = 0; j < number_of_locals; j++) {
                if (latency_profiling_ok && j == new_local_index) {
                    // Make sure our new local variable is not left out
                    put8(JVM_ITEM_Long);
                    continue;
                }
                rewriteVerificationTypeInfo(relocation_table);
            }
            u16 number_of_stack_items = get16();
            put16(number_of_stack_items);
            for (int j = 0; j < number_of_stack_items; j++) {
                rewriteVerificationTypeInfo(relocation_table);
            }
        }
    }

    // Patch attribute length and number of entries
    *(u32*)(_dst + attribute_start_idx - 4) = htonl(_dst_len - attribute_start_idx);
}

void BytecodeRewriter::rewriteVerificationTypeInfo(const u32* relocation_table) {
    u8 tag = get8();
    put8(tag);
    if (tag >= 7) {
        u16 offset = get16();
        if (tag == 8) {
            // Adjust ITEM_Uninitialized offset
            offset += relocation_table[offset];
        }
        put16(offset);
    }
}

void BytecodeRewriter::rewriteAttributes(Scope scope, u16 access_flags, u16 descriptor_index) {
    u16 attributes_count = get16();
    put16(attributes_count);

    for (int i = 0; i < attributes_count; i++) {
        u16 attribute_name_index = get16();
        put16(attribute_name_index);

        Constant* attribute_name = _cpool[attribute_name_index];
        if (scope == SCOPE_REWRITE_METHOD && attribute_name->equals("Code", 4)) {
            rewriteCode(access_flags, descriptor_index);
            continue;
        }

        u32 attribute_length = get32();
        put32(attribute_length);
        put(get(attribute_length), attribute_length);
    }
}

void BytecodeRewriter::rewriteCodeAttributes(const u32* relocation_table, int new_local_index) {
    u16 attributes_count = get16();
    put16(attributes_count);

    for (int i = 0; i < attributes_count; i++) {
        u16 attribute_name_index = get16();
        put16(attribute_name_index);

        Constant* attribute_name = _cpool[attribute_name_index];
        if (attribute_name->equals("LineNumberTable", 15)) {
            rewriteLineNumberTable(relocation_table);
            continue;
        } else if (attribute_name->equals("LocalVariableTable", 18) ||
                   attribute_name->equals("LocalVariableTypeTable", 22)) {
            rewriteLocalVariableTable(relocation_table, new_local_index);
            continue;
        } else if (attribute_name->equals("StackMapTable", 13)) {
            rewriteStackMapTable(relocation_table, new_local_index);
            continue;
        }

        u32 attribute_length = get32();
        put32(attribute_length);
        put(get(attribute_length), attribute_length);
    }
}

void BytecodeRewriter::rewriteMembers(Scope scope) {
    u16 members_count = get16();
    put16(members_count);

    for (int i = 0; i < members_count; i++) {
        u16 access_flags = get16();
        put16(access_flags);

        u16 name_index = get16();
        put16(name_index);

        u16 descriptor_index = get16();
        put16(descriptor_index);

        bool need_rewrite = scope == SCOPE_METHOD
            && _cpool[name_index]->matches(_target_method, _target_method_len)
            && (_target_signature == NULL || _cpool[descriptor_index]->matches(_target_signature, _target_signature_len));

        rewriteAttributes(need_rewrite ? SCOPE_REWRITE_METHOD : SCOPE_METHOD, access_flags, descriptor_index);
    }
}

bool BytecodeRewriter::rewriteClass() {
    u32 magic = get32();
    put32(magic);

    u32 version = get32();
    put32(version);

    _cpool_len = get16();
    put16(_cpool_len + EXTRA_CONSTANTS);

    const u8* cpool_start = _src;

    _cpool = new Constant*[_cpool_len];
    for (int i = 1; i < _cpool_len; i += _cpool[i]->slots()) {
        _cpool[i] = getConstant();
    }

    const u8* cpool_end = _src;
    put(cpool_start, cpool_end - cpool_start);

    putConstant(CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 2);
    putConstant(CONSTANT_Class, _cpool_len + 3);
    putConstant(CONSTANT_NameAndType, _cpool_len + 4, _cpool_len + 5);
    putConstant("one/profiler/Instrument");
    putConstant("recordEntry");
    putConstant("()V");

    putConstant(CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 7);
    putConstant(CONSTANT_NameAndType, _cpool_len + 8, _cpool_len + 9);
    putConstant("recordExit");
    putConstant("(J)V");

    putConstant(CONSTANT_Methodref, _cpool_len + 11, _cpool_len + 12);
    putConstant(CONSTANT_Class, _cpool_len + 13);
    putConstant(CONSTANT_NameAndType, _cpool_len + 14, _cpool_len + 15);
    putConstant("java/lang/System");
    putConstant("nanoTime");
    putConstant("()J");

    u16 access_flags = get16();
    put16(access_flags);

    u16 this_class = get16();
    put16(this_class);

    u16 class_name_index = _cpool[this_class]->info();
    if (!_cpool[class_name_index]->matches(_target_class, _target_class_len)) {
        return false;
    }

    u16 super_class = get16();
    put16(super_class);

    u16 interfaces_count = get16();
    put16(interfaces_count);
    put(get(interfaces_count * 2), interfaces_count * 2);

    rewriteMembers(SCOPE_FIELD);
    rewriteMembers(SCOPE_METHOD);
    rewriteAttributes(SCOPE_CLASS);

    return true;
}


char* Instrument::_target_class = NULL;
bool Instrument::_instrument_class_loaded = false;
u64 Instrument::_interval;
long Instrument::_latency;
volatile u64 Instrument::_calls;
volatile bool Instrument::_running;

Error Instrument::check(Arguments& args) {
    if (!_instrument_class_loaded) {
        if (!VM::loaded()) {
            return Error("Profiling event is not supported with non-Java processes");
        }

        JNIEnv* jni = VM::jni();
        JNINativeMethod native_method[2];
        native_method[0] = {(char*)"recordEntry", (char*)"()V", (void*)recordEntry};
        native_method[1] = {(char*)"recordExit", (char*)"(J)V", (void*)recordExit};

        jclass cls = jni->DefineClass(INSTRUMENT_NAME, NULL, (const jbyte*)INSTRUMENT_CLASS, INCBIN_SIZEOF(INSTRUMENT_CLASS));
        if (cls == NULL || jni->RegisterNatives(cls, native_method, 2) != 0) {
            jni->ExceptionDescribe();
            return Error("Could not load Instrument class");
        }

        _instrument_class_loaded = true;
    }

    if (args._latency >= 0) {
        if (args._interval > 0) {
            return Error("latency and interval cannot both be positive");
        }
    } else if (args._interval < 0) {
        return Error("interval must be positive");
    }

    return Error::OK;
}

Error Instrument::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    setupTargetClassAndMethod(args._event);
    if (args._latency >= 0) {
        _latency = args._latency;
        _interval = 0;
    } else {
        _latency = -1;
        _interval = args._interval ? args._interval : 1;
    }
    _calls = 0;
    _running = true;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
    retransformMatchedClasses(jvmti);

    return Error::OK;
}

void Instrument::stop() {
    _running = false;

    jvmtiEnv* jvmti = VM::jvmti();
    retransformMatchedClasses(jvmti);  // undo transformation
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
}

void Instrument::setupTargetClassAndMethod(const char* event) {
    char* new_class = strdup(event);
    *strrchr(new_class, '.') = 0;

    for (char* s = new_class; *s; s++) {
        if (*s == '.') *s = '/';
    }

    char* old_class = _target_class;
    _target_class = new_class;
    free(old_class);
}

void Instrument::retransformMatchedClasses(jvmtiEnv* jvmti) {
    jint class_count;
    jclass* classes;
    jvmtiError error;
    if ((error = jvmti->GetLoadedClasses(&class_count, &classes)) != 0) {
        Log::error("JVMTI error %d occurred while calling GetLoadedClasses, aborting", error);
        return;
    }

    jint matched_count = 0;
    size_t len = strlen(_target_class);
    bool wildcard_class = len == 1 && _target_class[0] == '*';
    for (int i = 0; i < class_count; i++) {
        char* signature;
        if (jvmti->GetClassSignature(classes[i], &signature, NULL) == 0) {
            if (signature[0] == 'L') {
                if (wildcard_class) {
                    jboolean modifiable;
                    if (jvmti->IsModifiableClass(classes[i], &modifiable) == 0 && modifiable) {
                        classes[matched_count++] = classes[i];
                    }
                } else if (strncmp(signature + 1, _target_class, len) == 0 && signature[len + 1] == ';') {
                    classes[matched_count++] = classes[i];
                }
            }
            jvmti->Deallocate((unsigned char*)signature);
        }
    }

    if (matched_count > 0) {
        jvmtiError error;
        if ((error = jvmti->RetransformClasses(matched_count, classes)) != 0) {
            char *error_name;
            jvmti->GetErrorName(error, &error_name);
            Log::error("%s occurred while calling RetransformClasses", error_name);
            jvmti->Deallocate((unsigned char*)error_name);
        }
        VM::jni()->ExceptionClear();
    }

    jvmti->Deallocate((unsigned char*)classes);
}

void JNICALL Instrument::ClassFileLoadHook(jvmtiEnv* jvmti, JNIEnv* jni,
                                           jclass class_being_redefined, jobject loader,
                                           const char* name, jobject protection_domain,
                                           jint class_data_len, const u8* class_data,
                                           jint* new_class_data_len, u8** new_class_data) {
    // Do not retransform if the profiling has stopped
    if (!_running) return;

    bool wildcard_class = _target_class[0] == '*' && strlen(_target_class) == 1;
    if (name == NULL || wildcard_class || strcmp(name, _target_class) == 0) {
        BytecodeRewriter rewriter(class_data, class_data_len, _target_class, _latency >= 0);
        rewriter.rewrite(new_class_data, new_class_data_len);
    }
}

void JNICALL Instrument::recordEntry(JNIEnv* jni, jobject unused) {
    if (!_enabled) return;

    if (_interval <= 1 || ((atomicInc(_calls) + 1) % _interval) == 0) {
        ExecutionEvent event(TSC::ticks());
        Profiler::instance()->recordSample(NULL, _interval, INSTRUMENTED_METHOD, &event);
    }
}

void JNICALL Instrument::recordExit(JNIEnv* jni, jobject unused, jlong startTimeNanos) {
    if (!_enabled) return;

    u64 duration = OS::nanotime() - (u64) startTimeNanos;
    if (duration >= _latency) {
        ExecutionEvent event((u64) startTimeNanos);
        Profiler::instance()->recordSample(NULL, duration, INSTRUMENTED_METHOD, &event);
    }
}
