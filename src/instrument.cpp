/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "assert.h"
#include "classfile_constants.h"
#include "incbin.h"
#include "log.h"
#include "os.h"
#include "profiler.h"
#include "tsc.h"
#include "vmEntry.h"
#include "instrument.h"

constexpr u16 MAX_CODE_LENGTH = 65534;

INCLUDE_HELPER_CLASS(INSTRUMENT_NAME, INSTRUMENT_CLASS, "one/profiler/Instrument")

static inline u16 alignUp4(u16 i) {
    return (i & ~3) + 4;
}

enum ConstantTag {
    // Available since JDK 17
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
        return _tag == JVM_CONSTANT_Long || _tag == JVM_CONSTANT_Double ? 2 : 1;
    }

    u16 info() {
        return (u16)_info[0] << 8 | (u16)_info[1];
    }

    const char* utf8() const {
        return (const char*) (_info + 2);
    }

    int length() {
        switch (_tag) {
            case JVM_CONSTANT_Utf8:
                return 2 + info();
            case JVM_CONSTANT_Integer:
            case JVM_CONSTANT_Float:
            case JVM_CONSTANT_Fieldref:
            case JVM_CONSTANT_Methodref:
            case JVM_CONSTANT_InterfaceMethodref:
            case JVM_CONSTANT_NameAndType:
            case JVM_CONSTANT_Dynamic:
            case JVM_CONSTANT_InvokeDynamic:
                return 4;
            case JVM_CONSTANT_Long:
            case JVM_CONSTANT_Double:
                return 8;
            case JVM_CONSTANT_Class:
            case JVM_CONSTANT_String:
            case JVM_CONSTANT_MethodType:
            case CONSTANT_Module:
            case CONSTANT_Package:
                return 2;
            case JVM_CONSTANT_MethodHandle:
                return 3;
            default:
                return 0;
        }
    }

    bool equals(const char* value, u16 len) {
        return _tag == JVM_CONSTANT_Utf8 && info() == len && memcmp(utf8(), value, len) == 0;
    }

    bool matches(const char* value, u16 len) {
        if (len > 0 && value[len - 1] == '*') {
            return _tag == JVM_CONSTANT_Utf8 && info() >= len - 1 && memcmp(utf8(), value, len - 1) == 0;
        }
        return equals(value, len);
    }

    static u8 parameterSlots(const char* method_sig) {
        u8 count = 0;
        for (const char* c = method_sig + 1; *c != ')'; ++c) {
            if (*c == 'L') {
                ++count;
                while (*(++c) != ';');
            } else if (*c == '[') {
                ++count;
                while (*(++c) == '[');
                if (*c == 'L') while (*(++c) != ';');
            } else if (*c == 'J' || *c == 'D') {
                count += 2;
            } else {
                ++count;
            }
        }
        return count;
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
    // Entry which does not track start time
    EXTRA_BYTECODES_SIMPLE_ENTRY = 4,
    // System.nanoTime and lstore
    EXTRA_BYTECODES_ENTRY = 8,
    // lload and recordExit(startTime)
    EXTRA_BYTECODES_EXIT = 8,
    // *load_i or *store_i to *load/*store
    EXTRA_BYTECODES_INDEXED = 4
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

    bool _latency_profiling;

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
        return get16(get(2));
    }

    u32 get32() {
        return get32(get(4));
    }

    u64 get64() {
        return OS::ntoh64(*(u64*)get(8));
    }

    static u16 get16(const u8* ptr) {
        return ntohs(*(u16*)ptr);
    }

    static u32 get32(const u8* ptr) {
        return ntohl(*(u32*)ptr);
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
        put16(alloc(2), v);
    }

    void put32(u32 v) {
        put32(alloc(4), v);
    }

    void put64(u64 v) {
        *(u64*)alloc(8) = OS::hton64(v);
    }

    static void put16(u8* ptr, u16 v) {
        *(u16*)ptr = htons(v);
    }

    static void put32(u8* ptr, u32 v) {
        *(u32*)ptr = htonl(v);
    }

    // Utilities

    void putConstant(const char* value) {
        u16 len = strlen(value);
        put8(JVM_CONSTANT_Utf8);
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
    u16 rewriteCodeForLatency(const u8* code, u16 code_length, u8 start_time_loc_index, u16* relocation_table);
    void rewriteLineNumberTable(const u16* relocation_table);
    void rewriteLocalVariableTable(const u16* relocation_table, int new_local_index);
    void rewriteStackMapTable(const u16* relocation_table, int new_local_index);
    u8 rewriteVerificationTypeInfo(const u16* relocation_table);
    void rewriteAttributes(Scope scope, u16 access_flags = 0, u16 descriptor_index = 0);
    void rewriteCodeAttributes(const u16* relocation_table, int new_local_index);
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

    static u16 instructionBytes(const u8* code, u16 index) {
        static constexpr unsigned char OPCODE_LENGTH[JVM_OPC_MAX+1] = JVM_OPCODE_LENGTH_INITIALIZER;
        u8 opcode = code[index];
        switch (opcode) {
            case JVM_OPC_wide:
                return code[index + 1] == JVM_OPC_iinc ? 6 : 4;
            case JVM_OPC_tableswitch: {
                u16 default_index = alignUp4(index);
                int32_t l = get32(code + default_index + 4);
                int32_t h = get32(code + default_index + 8);
                u16 branches_count = h - l + 1;
                return default_index - index + (3 + branches_count) * 4;
            }
            case JVM_OPC_lookupswitch: {
                u16 default_index = alignUp4(index);
                u16 npairs = (u16) get32(code + default_index + 4);
                return default_index - index + (npairs + 1) * 8;
            }
            default:
                assert(opcode < JVM_OPC_MAX+1);
                return OPCODE_LENGTH[opcode];
        }
    }

    // Return the new offset delta
    static u16 updateCurrentFrame(int32_t& current_frame_old, int32_t& current_frame_new, 
                        u16 offset_delta_old, const u16* relocation_table) {
        current_frame_old += offset_delta_old + 1;
        long previous_frame_new = current_frame_new;
        current_frame_new = current_frame_old + relocation_table[current_frame_old];
        return current_frame_new - previous_frame_new - 1;
    }
};

static inline bool isFunctionExit(u8 opcode) {
    return opcode >= JVM_OPC_ireturn && opcode <= JVM_OPC_return;
}

static inline bool isNarrowJump(u8 opcode) {
    return (opcode >= JVM_OPC_ifeq && opcode <= JVM_OPC_jsr) || opcode == JVM_OPC_ifnull ||
            opcode == JVM_OPC_ifnonnull || opcode == JVM_OPC_goto;
}

static inline bool isWideJump(u8 opcode) {
    return opcode == JVM_OPC_goto_w || opcode == JVM_OPC_jsr_w;
}

void BytecodeRewriter::rewriteCode(u16 access_flags, u16 descriptor_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    int code_begin = _dst_len;

    u16 max_stack = get16();
    put16(max_stack + (_latency_profiling ? 2 : 0));

    u16 max_locals = get16();
    put16(max_locals + (_latency_profiling ? 2 : 0));

    u32 code_length_32 = get32();
    assert(code_length_32 <= MAX_CODE_LENGTH);
    u16 code_length = (u16) code_length_32;

    const u8* code = get(code_length);
    u32 code_length_idx = _dst_len;
    put32(code_length); // to be fixed later

    // For each index in the (original) bytecode, this holds the rightwards offset
    // in the modified bytecode.
    // This is code_length + 1 for convenience: sometimes we need to access the
    // code_length-ith index to refer to the first position after the code array
    // (i.e. LocalVariableTable).
    u16* relocation_table = new u16[code_length + 1];

    // This contains the byte index, considering that long and double take two slots
    int new_local_index = -1;
    u16 relocation;
    if (_latency_profiling) {
        // Find function signature (parameters + return value)
        const char* sig = _cpool[descriptor_index]->utf8();
        while (*sig != 0 && *sig != '(') sig++;

        u8 parameters_count = Constant::parameterSlots(sig);
        bool is_non_static = (access_flags & JVM_ACC_STATIC) == 0;
        new_local_index = parameters_count + is_non_static;
        relocation = rewriteCodeForLatency(code, code_length, new_local_index, relocation_table);
    } else {
        // invokestatic "one/profiler/Instrument.recordEntry()V"
        put8(JVM_OPC_invokestatic);
        put16(_cpool_len);
        // nop ensures that tableswitch/lookupswitch needs no realignment
        put8(JVM_OPC_nop);
        relocation = EXTRA_BYTECODES_SIMPLE_ENTRY;

        // The rest of the code is unchanged
        put(code, code_length);
        for (u16 i = 0; i <= code_length; ++i) relocation_table[i] = relocation;
    }

    // Fix code length, we now know the real relocation
    put32(_dst + code_length_idx, code_length + relocation);

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
    put32(_dst + code_begin - 4, _dst_len - code_begin);
}

// Return the relocation after the last byte of code
u16 BytecodeRewriter::rewriteCodeForLatency(const u8* code, u16 code_length, u8 start_time_loc_index, u16* relocation_table) {
    // Method start is relocated
    u16 current_relocation = EXTRA_BYTECODES_ENTRY;

    u32 code_start = _dst_len;

    // invokestatic "java/lang/System.nanoTime()J"
    put8(JVM_OPC_invokestatic);
    put16(_cpool_len + 10);

    put8(JVM_OPC_lstore);
    put8(start_time_loc_index);

    // nop ensures that tableswitch/lookupswitch needs no realignment
    put16(JVM_OPC_nop);
    put8(JVM_OPC_nop);

    // Low 16 bytes: jump base index
    // High 16 bytes: jump offset index
    // This supports narrow and wide jumps, as well as tableswitch and lookupswitch
    std::vector<u32> jumps;
    // First scan: fill relocation_table and rewrite code.
    for (u16 i = 0; i < code_length; /* incremented with instructionBytes */) {
        u8 opcode = code[i];

        if (isFunctionExit(opcode)) {
            put8(JVM_OPC_lload);
            put8(start_time_loc_index);
            put16(JVM_OPC_nop);

            // invokestatic "one/profiler/Instrument.recordExit(J)V"
            put8(JVM_OPC_invokestatic);
            put16(_cpool_len + 6);
            put8(JVM_OPC_nop);
        } else if (isNarrowJump(opcode) || isWideJump(opcode)) {
            jumps.push_back((i + 1U) << 16 | i);
        } else if (opcode == JVM_OPC_tableswitch) {
            // 'default' lies after the padding
            u16 default_index = alignUp4(i);
            // 4 bytes: default
            jumps.push_back((u32) default_index << 16 | i);
            // 4 bytes: low
            int32_t l = get32(code + default_index + 4);
            // 4 bytes: high
            int32_t h = get32(code + default_index + 8);
            assert(h - l + 1 >= 0);
            assert(h - l + 1 <= 0xFFFF);
            u16 branches_base_index = default_index + 12;
            for (u16 c = 0; c < (u16) (h - l + 1); ++c) {
                jumps.push_back((branches_base_index + c * 4) << 16 | i);
            }
        } else if (opcode == JVM_OPC_lookupswitch) {
            // 'default' lies after the padding
            u16 default_index = alignUp4(i);
            // 4 bytes: default
            jumps.push_back((u32) default_index << 16 | i);
            // 4 bytes: npairs
            u16 npairs = (u16) get32(code + default_index + 4);
            u16 branches_base_index = default_index + 12;
            for (u16 c = 0; c < npairs; ++c) {
                u16 pair_base = branches_base_index + c * 8;
                // 4 bytes: match
                // 4 bytes: offset
                jumps.push_back((u32) pair_base << 16 | i);
            }
        }

        u16 bc = instructionBytes(code, i);
        for (u16 j = 0; j < bc; ++j) relocation_table[i + j] = current_relocation;
        put(code + i, bc);
        i += bc;

        // current_relocation should be incremented for addresses after the
        // current instruction: any instruction referring to the current one
        // (e.g. a jump) should now target our invokestatic, otherwise it will
        // be skipped.
        if (isFunctionExit(opcode)) {
            current_relocation += EXTRA_BYTECODES_EXIT;
        } else if ((opcode >= JVM_OPC_iload && opcode <= JVM_OPC_aload) ||
                   (opcode >= JVM_OPC_istore && opcode <= JVM_OPC_astore)) {
            u8 index = code[i-1];
            if (index >= start_time_loc_index) {
                index += 2;
                _dst[_dst_len - 1] = index;
            }
        } else if ((opcode >= JVM_OPC_iload_0 && opcode <= JVM_OPC_aload_3) ||
                   (opcode >= JVM_OPC_istore_0 && opcode <= JVM_OPC_astore_3)) {
            u8 base = opcode <= JVM_OPC_aload_3 ? JVM_OPC_iload_0 : JVM_OPC_istore_0;
            u8 index = (opcode - base) % 4;
            if (index >= start_time_loc_index) {
                index += 2;
                if (index <= 3) {
                    _dst[_dst_len - 1] = opcode + 2;
                } else {
                    u8 new_opcode = base - (JVM_OPC_iload_0 - JVM_OPC_iload) + (opcode - base) / 4;
                    _dst[_dst_len - 1] = new_opcode;
                    put8(index);
                    put8(JVM_OPC_nop);
                    put8(JVM_OPC_nop);
                    put8(JVM_OPC_nop);
                    current_relocation += EXTRA_BYTECODES_INDEXED;
                }
            }
        } else if (opcode == JVM_OPC_iinc) {
            u8 index = code[i-2];
            if (index >= start_time_loc_index) {
                assert(index <= 253);
                index += 2;
                _dst[_dst_len - 2] = index;
            }
        } else if (opcode == JVM_OPC_wide) {
            u16 back = bc - 2;
            u16 index = get16(code + i - back);
            if (index >= start_time_loc_index) {
                index += 2;
                put16(_dst + _dst_len - back, index);
            }
        }
    }

    if (current_relocation > MAX_CODE_LENGTH - code_length) {
        Log::warn("Method %s.%s is too large for instrumentation", _target_class, _target_method);
        _dst_len = code_start;
        put(code, code_length);
        memset(relocation_table, 0, sizeof(relocation_table[0]) * (code_length + 1));
        return 0;
    }

    // Second scan (jumps only): fix the jump offset using information in relocation_table.
    for (u32 jump : jumps) {
        u16 old_jump_base_idx = (u16) jump;
        u16 old_jump_offset_idx = (u16) (jump >> 16);

        u16 new_jump_base_idx = old_jump_base_idx + relocation_table[old_jump_base_idx];
        u8* new_jump_base_ptr = _dst + code_start + new_jump_base_idx;
        assert(code[old_jump_base_idx] == *new_jump_base_ptr);

        bool is_narrow = isNarrowJump(*new_jump_base_ptr);
        int32_t old_offset;
        if (is_narrow) {
            old_offset = (int32_t) (int16_t) get16(code + old_jump_offset_idx);
        } else {
            old_offset = (int32_t) get32(code + old_jump_offset_idx);
        }

        u16 old_jump_target = (u16) (old_jump_base_idx + old_offset);
        int32_t new_offset = old_jump_target + relocation_table[old_jump_target] - new_jump_base_idx;

        u16 new_jump_offset_idx = old_jump_offset_idx + relocation_table[old_jump_offset_idx];
        u8* new_jump_offset_ptr = _dst + code_start + new_jump_offset_idx;
        if (is_narrow) {
            if (new_offset != (int16_t)new_offset) {
                Log::warn("Jump overflow, aborting instrumentation of %s.%s", _target_class, _target_method);
                _dst_len = code_start;
                put(code, code_length);
                memset(relocation_table, 0, sizeof(relocation_table[0]) * (code_length + 1));
                return 0;
            }
            put16(new_jump_offset_ptr, new_offset);
        } else {
            put32(new_jump_offset_ptr, (u32) new_offset);
        }
    }

    relocation_table[code_length] = current_relocation;
    return current_relocation;
}

void BytecodeRewriter::rewriteLineNumberTable(const u16* relocation_table) {
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

void BytecodeRewriter::rewriteLocalVariableTable(const u16* relocation_table, int new_local_index) {
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
        if (new_local_index >= 0 && index >= new_local_index) {
            // The new variable is a long
            index += 2;
        }
        put16(index);
    }
}

// new_local_index is the byte index, considering that long and double take two slots
void BytecodeRewriter::rewriteStackMapTable(const u16* relocation_table, int new_local_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    // Current instruction index in the old code
    int32_t current_frame_old = -1;
    // And in the new code
    int32_t current_frame_new = -1;

    u32 attribute_start_idx = _dst_len;
    u16 number_of_entries = get16();

    // Latency profiling may have bailed out
    bool latency_profiling_ok = new_local_index >= 0;
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
                new_frame_type = (frame_type & ~63) | new_offset_delta;
            } else {
                // Convert to same_locals_1_stack_item_frame_extended or same_frame_extended
                new_frame_type = frame_type > 63 ? 247 : 251;
                put16(new_offset_delta);
            }
            // Patch frame type
            _dst[stack_map_frame_idx] = new_frame_type;
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
            u16 locals_count_old = get16();
            put16(locals_count_old + latency_profiling_ok);

            u16 locals_idx_old = 0;
            if (latency_profiling_ok) {
                u8 current_byte_count = 0;
                while (current_byte_count < new_local_index) {
                    u8 tag = rewriteVerificationTypeInfo(relocation_table);
                    current_byte_count += (tag == JVM_ITEM_Long || tag == JVM_ITEM_Double ? 2 : 1);
                    ++locals_idx_old;
                }

                assert(current_byte_count == new_local_index);
                assert(locals_idx_old <= locals_count_old);
                // Make sure our new local variable is not left out
                put8(JVM_ITEM_Long);
            }

            // Write the remaining locals
            for (; locals_idx_old < locals_count_old; ++locals_idx_old) {
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
    put32(_dst + attribute_start_idx - 4, _dst_len - attribute_start_idx);
}

u8 BytecodeRewriter::rewriteVerificationTypeInfo(const u16* relocation_table) {
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
    return tag;
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

void BytecodeRewriter::rewriteCodeAttributes(const u16* relocation_table, int new_local_index) {
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
        assert(_cpool[name_index]->tag() == JVM_CONSTANT_Utf8);

        u16 descriptor_index = get16();
        put16(descriptor_index);
        assert(_cpool[descriptor_index]->tag() == JVM_CONSTANT_Utf8);

        bool need_rewrite = scope == SCOPE_METHOD
            && (access_flags & JVM_ACC_NATIVE) == 0
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

    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 2);
    putConstant(JVM_CONSTANT_Class, _cpool_len + 3);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 4, _cpool_len + 5);
    putConstant("one/profiler/Instrument");
    putConstant("recordEntry");
    putConstant("()V");

    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 7);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 8, _cpool_len + 9);
    putConstant("recordExit");
    putConstant("(J)V");

    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 11, _cpool_len + 12);
    putConstant(JVM_CONSTANT_Class, _cpool_len + 13);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 14, _cpool_len + 15);
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

    if (args._latency < 0 && args._interval < 0) {
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
    _latency = args._latency;
    _interval = args._interval ? args._interval : 1;
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
        char* error_name;
        jvmti->GetErrorName(error, &error_name);
        Log::error("%s occurred while calling GetLoadedClasses, aborting", error_name);
        jvmti->Deallocate((unsigned char*)error_name);
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
            char* error_name;
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

    if (shouldRecordSample()) {
        ExecutionEvent event(TSC::ticks());
        Profiler::instance()->recordSample(NULL, _interval, INSTRUMENTED_METHOD, &event);
    }
}

void JNICALL Instrument::recordExit(JNIEnv* jni, jobject unused, jlong startTimeNanos) {
    if (!_enabled) return;

    u64 duration_ns = OS::nanotime() - (u64) startTimeNanos;
    u64 now_ticks = TSC::ticks();
    if (duration_ns >= _latency && shouldRecordSample()) {
        u64 duration_ticks = (u64) ((double) duration_ns * TSC::frequency() / NANOTIME_FREQ);
        MethodTraceEvent event(now_ticks - duration_ticks, duration_ticks);
        Profiler::instance()->recordSample(NULL, duration_ns, METHOD_TRACE, &event);
    }
}
