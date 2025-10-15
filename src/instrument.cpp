/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
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

#define PROFILER_PACKAGE "one/profiler/"
static constexpr u32 PROFILER_PACKAGE_LEN = 13;

INCLUDE_HELPER_CLASS(INSTRUMENT_NAME, INSTRUMENT_CLASS, "one/profiler/Instrument")

constexpr u16 MAX_CODE_LENGTH = 65534;
constexpr Latency NO_LATENCY = -1;

enum class Result {
    OK,
    METHOD_TOO_LARGE,
    JUMP_OVERFLOW,
    BAD_FULL_FRAME,
    PROFILER_CLASS,
    ABORTED
};

static inline u16 alignUp4(u16 i) {
    return (i & ~3) + 4;
}

static bool matchesPattern(const char* value, size_t len, const std::string& pattern) {
    if (len == 0 || pattern.empty()) return false;
    return (
        // wildcard match
        (
            pattern[pattern.length() - 1] == '*' &&
            len >= pattern.length() - 1 &&
            memcmp(pattern.c_str(), value, pattern.length() - 1) == 0
        ) ||
        // full match
        memcmp(pattern.c_str(), value, len) == 0
    );
}

static const MethodTargets* findMethodTargets(const Targets* targets, const char* class_name, size_t len) {
    for (const auto& target : *targets) {
        if (matchesPattern(class_name, len, target.first)) {
            return &target.second;
        }
    }
    return nullptr;
}

enum ConstantTag {
    // Available since JDK 11
    CONSTANT_Dynamic = 17,
    // Available since JDK 17
    CONSTANT_Module = 19,
    CONSTANT_Package = 20
};

class Constant {
  private:
    u8 _tag;
    u8 _info[2];

  public:
    u8 tag() const {
        return _tag;
    }

    int slots() const {
        return _tag == JVM_CONSTANT_Long || _tag == JVM_CONSTANT_Double ? 2 : 1;
    }

    u16 info() const {
        return (u16)_info[0] << 8 | (u16)_info[1];
    }

    const char* utf8() const {
        assert(_tag == JVM_CONSTANT_Utf8);
        return (const char*) (_info + 2);
    }

    std::string toString() const {
        return std::string(utf8(), info());
    }

    int length() const {
        switch (_tag) {
            case JVM_CONSTANT_Utf8:
                return 2 + info();
            case JVM_CONSTANT_Integer:
            case JVM_CONSTANT_Float:
            case JVM_CONSTANT_Fieldref:
            case JVM_CONSTANT_Methodref:
            case JVM_CONSTANT_InterfaceMethodref:
            case JVM_CONSTANT_NameAndType:
            case CONSTANT_Dynamic:
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

    bool equals(const char* value, u16 len) const {
        return info() == len && memcmp(utf8(), value, len) == 0;
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
};

enum PatchConstants {
    // Entry which does not track start time
    EXTRA_BYTECODES_SIMPLE_ENTRY = 4,
    EXTRA_BYTECODES_ENTRY = 8,
    EXTRA_BYTECODES_EXIT = 8,
    // *load_i or *store_i to *load/*store
    EXTRA_BYTECODES_INDEXED = 4
};

class BytecodeRewriter {
  private:
    const u8* _src;
    const u8* _src_limit;

    const Constant* _class_name;
    const Constant* _method_name;

    u8* _dst;
    u32 _dst_len;
    u32 _dst_capacity;

    Constant** _cpool;
    u16 _cpool_len;

    // one/profiler/Instrument.recordEntry()V
    u16 _recordEntry_cpool_idx;
    // one/profiler/Instrument.recordExit(JJ)V
    u16 _recordExit_cpool_idx;
    // one/profiler/Instrument.recordExit(J)V
    u16 _recordExit_latency0_cpool_idx;
    // java/lang/System.nanoTime()J
    u16 _nanoTime_cpool_idx;

    // Maps latency to the index in the constant pool
    std::unordered_map<Latency, u16> _latency_cpool_idx;

    const Targets* const _targets;
    const MethodTargets* _method_targets;

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

    void putLongConstant(u64 l) {
        put8(JVM_CONSTANT_Long);
        put32((u32) (l >> 32));
        put32((u32) l);
    }

    // BytecodeRewriter

    Result rewriteCode(u16 access_flags, u16 descriptor_index, Latency latency);
    Result rewriteCodeForLatency(const u8* code, u16 code_length, u8 start_time_loc_index, u16* relocation_table, Latency latency);
    void rewriteLineNumberTable(const u16* relocation_table);
    void rewriteLocalVariableTable(const u16* relocation_table, int new_local_index);
    Result rewriteStackMapTable(const u16* relocation_table, int new_local_index);
    u8 rewriteVerificationTypeInfo(const u16* relocation_table);
    Result rewriteMethod(u16 access_flags, u16 descriptor_index, Latency latency);
    Result rewriteAttributes(u16 access_flags = 0, u16 descriptor_index = 0);
    Result rewriteCodeAttributes(const u16* relocation_table, int new_local_index);
    Result rewriteMembers(Scope scope);
    Result rewriteClass();

  public:
    BytecodeRewriter(const u8* class_data, int class_data_len, const Targets* targets, const MethodTargets* method_targets) :
        _src(class_data),
        _src_limit(class_data + class_data_len),
        _dst(NULL),
        _dst_len(0),
        _dst_capacity(class_data_len + 400),
        _cpool(NULL),
        _class_name(nullptr),
        _method_name(nullptr),
        _targets(targets),
        _method_targets(method_targets) {}

    ~BytecodeRewriter() {
        delete[] _cpool;
    }

    void rewrite(u8** new_class_data, int* new_class_data_len) {
        if (VM::jvmti()->Allocate(_dst_capacity, &_dst) != 0) {
            return;
        }

        Result res = rewriteClass();
        if (res == Result::OK) {
            *new_class_data = _dst;
            *new_class_data_len = _dst_len;
            return;
        }

        VM::jvmti()->Deallocate(_dst);

        std::string class_name = _class_name->toString();
        std::string method_name = _method_name ? _method_name->toString() : "n/a";
        switch (res) {
            case Result::METHOD_TOO_LARGE:
                Log::warn("Method too large: %s.%s", class_name.c_str(), method_name.c_str());
                break;
            case Result::BAD_FULL_FRAME:
                Log::warn("Unsupported full_frame: %s.%s", class_name.c_str(), method_name.c_str());
                break;
            case Result::JUMP_OVERFLOW:
                Log::warn("Jump overflow: %s.%s", class_name.c_str(), method_name.c_str());
                break;
            case Result::PROFILER_CLASS:
                Log::trace("Skipping instrumentation of %s: internal profiler class", class_name.c_str());
                break;
            case Result::ABORTED:
                // Nothing to do
                break;
            default:
                break;
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
    return (opcode >= JVM_OPC_ifeq && opcode <= JVM_OPC_jsr) ||
            opcode == JVM_OPC_ifnull || opcode == JVM_OPC_ifnonnull || opcode == JVM_OPC_goto;
}

static inline bool isWideJump(u8 opcode) {
    return opcode == JVM_OPC_goto_w || opcode == JVM_OPC_jsr_w;
}

Result BytecodeRewriter::rewriteCode(u16 access_flags, u16 descriptor_index, Latency latency) {
    u32 attribute_length = get32();
    put32(attribute_length);

    int code_begin = _dst_len;

    u16 max_stack = get16();
    put16(max_stack + (latency >= 0 ? 4 : 0));

    u16 max_locals = get16();
    put16(max_locals + (latency >= 0 ? 2 : 0));

    u32 code_length_32 = get32();
    assert(code_length_32 <= MAX_CODE_LENGTH);
    u16 code_length = (u16) code_length_32;

    const u8* code = get(code_length);
    u32 code_length_idx = _dst_len;
    put32(code_length); // to be patched later

    // For each index in the (original) bytecode, this holds the rightwards offset
    // in the modified bytecode.
    // This is code_length + 1 for convenience: sometimes we need to access the
    // code_length-ith index to refer to the first position after the code array
    // (i.e. LocalVariableTable).
    u16* relocation_table = new u16[code_length + 1];

    // This contains the byte index, considering that long and double take two slots
    int new_local_index = -1;
    if (latency == NO_LATENCY) {
        put8(JVM_OPC_invokestatic);
        put16(_recordEntry_cpool_idx);
        // nop ensures that tableswitch/lookupswitch needs no realignment
        put8(JVM_OPC_nop);

        // The rest of the code is unchanged
        put(code, code_length);
        for (u16 i = 0; i <= code_length; ++i) relocation_table[i] = EXTRA_BYTECODES_SIMPLE_ENTRY;
    } else {
        assert(latency >= 0);

        // Find function signature (parameters + return value)
        const char* sig = _cpool[descriptor_index]->utf8();
        while (*sig != 0 && *sig != '(') sig++;

        u8 parameters_count = Constant::parameterSlots(sig);
        bool is_non_static = (access_flags & JVM_ACC_STATIC) == 0;
        new_local_index = parameters_count + is_non_static;

        Result res = rewriteCodeForLatency(code, code_length, new_local_index, relocation_table, latency);
        if (res != Result::OK) {
            delete[] relocation_table;
            return res;
        }
    }

    // Fix code length, we now know the real relocation
    put32(_dst + code_length_idx, code_length + relocation_table[code_length]);

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

    Result res = rewriteCodeAttributes(relocation_table, new_local_index);
    delete[] relocation_table;
    if (res != Result::OK) return res;

    // Patch attribute length
    put32(_dst + code_begin - 4, _dst_len - code_begin);
    return Result::OK;
}

Result BytecodeRewriter::rewriteCodeForLatency(const u8* code, u16 code_length, u8 start_time_loc_index, u16* relocation_table, Latency latency) {
    // Method start is relocated
    u16 current_relocation = EXTRA_BYTECODES_ENTRY;

    u32 code_start = _dst_len;

    put8(JVM_OPC_invokestatic);
    put16(_nanoTime_cpool_idx);

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

            if (latency > 0) {
                put8(JVM_OPC_ldc2_w);
                put16(_latency_cpool_idx[latency]);
            } else {
                put8(JVM_OPC_nop);
                put8(JVM_OPC_nop);
                put8(JVM_OPC_nop);
            }

            put8(JVM_OPC_invokestatic);
            put16(latency == 0 ? _recordExit_latency0_cpool_idx : _recordExit_cpool_idx);
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

        relocation_table[i] = current_relocation;
        u16 bc = instructionBytes(code, i);
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
        return Result::METHOD_TOO_LARGE;
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

        u16 new_jump_offset_idx = old_jump_offset_idx + relocation_table[old_jump_base_idx];
        u8* new_jump_offset_ptr = _dst + code_start + new_jump_offset_idx;
        if (is_narrow) {
            if (new_offset != (int16_t)new_offset) {
                return Result::JUMP_OVERFLOW;
            }
            put16(new_jump_offset_ptr, new_offset);
        } else {
            put32(new_jump_offset_ptr, (u32) new_offset);
        }
    }

    relocation_table[code_length] = current_relocation;
    return Result::OK;
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
Result BytecodeRewriter::rewriteStackMapTable(const u16* relocation_table, int new_local_index) {
    u32 attribute_length = get32();
    put32(attribute_length);

    // Current instruction index in the old code
    int32_t current_frame_old = -1;
    // And in the new code
    int32_t current_frame_new = -1;

    u32 attribute_start_idx = _dst_len;
    u16 number_of_entries = get16();

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
                while (locals_idx_old < locals_count_old && current_byte_count < new_local_index) {
                    u8 tag = rewriteVerificationTypeInfo(relocation_table);
                    current_byte_count += (tag == JVM_ITEM_Long || tag == JVM_ITEM_Double ? 2 : 1);
                    ++locals_idx_old;
                }

                if (current_byte_count != new_local_index) {
                    return Result::BAD_FULL_FRAME;
                }
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
    return Result::OK;
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

Result BytecodeRewriter::rewriteMethod(u16 access_flags, u16 descriptor_index, Latency latency) {
    u16 attributes_count = get16();
    put16(attributes_count);

    for (int i = 0; i < attributes_count; i++) {
        u16 attribute_name_index = get16();
        put16(attribute_name_index);

        Constant* attribute_name = _cpool[attribute_name_index];
        if (attribute_name->equals("Code", 4)) {
            Result res = rewriteCode(access_flags, descriptor_index, latency);
            if (res != Result::OK) return res;
            continue;
        }

        u32 attribute_length = get32();
        put32(attribute_length);
        put(get(attribute_length), attribute_length);
    }
    return Result::OK;
}

Result BytecodeRewriter::rewriteAttributes(u16 access_flags, u16 descriptor_index) {
    u16 attributes_count = get16();
    put16(attributes_count);

    for (int i = 0; i < attributes_count; i++) {
        // attribute_name_index
        put16(get16());

        u32 attribute_length = get32();
        put32(attribute_length);
        put(get(attribute_length), attribute_length);
    }
    return Result::OK;
}

Result BytecodeRewriter::rewriteCodeAttributes(const u16* relocation_table, int new_local_index) {
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
            Result res = rewriteStackMapTable(relocation_table, new_local_index);
            if (res != Result::OK) return res;
            continue;
        }

        u32 attribute_length = get32();
        put32(attribute_length);
        put(get(attribute_length), attribute_length);
    }
    return Result::OK;
}

static bool findLatency(const MethodTargets* method_targets, const std::string&& method_name,
                        const std::string&& method_desc, Latency& latency) {
    const std::string method = method_name + method_desc;
    auto it = method_targets->lower_bound(method);
    if (it == method_targets->end()) --it;

    while (true) {
        if (
            // Try to match the whole method descriptor
            matchesPattern(method.c_str(), method.length(), it->first) ||
            // Or, try to match only the name if the pattern does not contain the signature
            (method_name.length() == it->first.length() &&
                memcmp(method_name.c_str(), it->first.c_str(), it->first.length()) == 0)
        ) {
            latency = it->second;
            return true;
        }

        if (it == method_targets->begin()) return false;
        --it;
    }
}

Result BytecodeRewriter::rewriteMembers(Scope scope) {
    u16 members_count = get16();
    put16(members_count);

    for (int i = 0; i < members_count; i++) {
        u16 access_flags = get16();
        put16(access_flags);

        u16 name_index = get16();
        put16(name_index);

        u16 descriptor_index = get16();
        put16(descriptor_index);

        if (scope == SCOPE_METHOD) {
            _method_name = _cpool[name_index];
            Latency latency;
            if ((access_flags & JVM_ACC_NATIVE) == 0 &&
                findLatency(_method_targets, _cpool[name_index]->toString(),
                            _cpool[descriptor_index]->toString(), latency)
            ) {
                Result res = rewriteMethod(access_flags, descriptor_index, latency);
                if (res != Result::OK) return res;
                continue;
            }
        }

        Result res = rewriteAttributes(access_flags, descriptor_index);
        if (res != Result::OK) return res;
    }
    return Result::OK;
}

Result BytecodeRewriter::rewriteClass() {
    u32 magic = get32();
    put32(magic);

    u32 version = get32();
    put32(version);

    _cpool_len = get16();
    u32 cpool_len_idx = _dst_len;
    put16(0); // to be patched later

    const u8* cpool_start = _src;

    _cpool = new Constant*[_cpool_len];
    for (int i = 1; i < _cpool_len; i += _cpool[i]->slots()) {
        _cpool[i] = getConstant();
    }

    const u8* cpool_end = _src;
    put(cpool_start, cpool_end - cpool_start);

    _recordEntry_cpool_idx = _cpool_len;
    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 2);
    putConstant(JVM_CONSTANT_Class, _cpool_len + 3);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 4, _cpool_len + 5);
    putConstant("one/profiler/Instrument");
    putConstant("recordEntry");
    putConstant("()V");

    _recordExit_cpool_idx = _cpool_len + 6;
    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 7);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 8, _cpool_len + 9);
    putConstant("recordExit");
    putConstant("(JJ)V");

    _recordExit_latency0_cpool_idx = _cpool_len + 10;
    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 1, _cpool_len + 11);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 8, _cpool_len + 12);
    putConstant("(J)V");

    _nanoTime_cpool_idx = _cpool_len + 13;
    putConstant(JVM_CONSTANT_Methodref, _cpool_len + 14, _cpool_len + 15);
    putConstant(JVM_CONSTANT_Class, _cpool_len + 16);
    putConstant(JVM_CONSTANT_NameAndType, _cpool_len + 17, _cpool_len + 18);
    putConstant("java/lang/System");
    putConstant("nanoTime");
    putConstant("()J");

    // Flushed later to the buffer, after latency-related constants are written to the cpool
    u16 access_flags = get16();
    u16 this_class = get16();

    u16 class_name_index = _cpool[this_class]->info();
    _class_name = _cpool[class_name_index];
    // We should not instrument classes from one.profiler.* to avoid infinite recursion
    if (_class_name->info() >= PROFILER_PACKAGE_LEN &&
        strncmp(_class_name->utf8(), PROFILER_PACKAGE, PROFILER_PACKAGE_LEN) == 0) {
        return Result::PROFILER_CLASS;
    }

    if (_method_targets == nullptr) {
        _method_targets = findMethodTargets(_targets, _class_name->utf8(), _class_name->info());
    }
    if (_method_targets == nullptr) {
        // The class name in the cpool didn't match any of the targets,
        // no need to do anything
        return Result::ABORTED;
    }

    u16 new_cpool_len = _cpool_len + 19;
    for (const auto& target : *_method_targets) {
        Latency latency = target.second;
        // latency == 0 does not need a spot in the map
        if (latency > 0 && _latency_cpool_idx[latency] == 0) {
            _latency_cpool_idx[latency] = new_cpool_len;
            putLongConstant(latency);
            new_cpool_len += 2;
        }
    }
    put16(_dst + cpool_len_idx, new_cpool_len);

    put16(access_flags);
    put16(this_class);

    u16 super_class = get16();
    put16(super_class);

    u16 interfaces_count = get16();
    put16(interfaces_count);
    put(get(interfaces_count * 2), interfaces_count * 2);

    Result res = rewriteMembers(SCOPE_FIELD);
    if (res != Result::OK) return res;
    res = rewriteMembers(SCOPE_METHOD);
    if (res != Result::OK) return res;
    res = rewriteAttributes();

    return res;
}


Targets Instrument::_targets;
bool Instrument::_instrument_class_loaded = false;
Latency Instrument::_interval;
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
        native_method[1] = {(char*)"recordExit0", (char*)"(J)V", (void*)recordExit0};

        jclass cls = jni->DefineClass(INSTRUMENT_NAME, NULL, (const jbyte*)INSTRUMENT_CLASS, INCBIN_SIZEOF(INSTRUMENT_CLASS));
        if (cls == NULL || jni->RegisterNatives(cls, native_method, 2) != 0) {
            jni->ExceptionDescribe();
            return Error("Could not load Instrument class");
        }

        _instrument_class_loaded = true;
    }

    return Error::OK;
}

Error Instrument::start(Arguments& args) {
    Error error = check(args);
    if (error) return error;

    error = setupTargetClassAndMethod(args);
    if (error) return error;

    bool no_cpu_profiling = (args._event == NULL) ^ args._trace.empty();
    _interval = no_cpu_profiling && args._interval ? args._interval : 1;
    _calls = 0;
    _running = true;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);
    retransformMatchedClasses(jvmti);

    return Error::OK;
}

void Instrument::stop() {
    if (!_running) return;
    _running = false;
    if (VM::isTerminating()) return;

    jvmtiEnv* jvmti = VM::jvmti();
    retransformMatchedClasses(jvmti);  // undo transformation
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL);

    _targets.clear();
}

Error addTarget(Targets& targets, const char* s, Latency default_latency) {
    // Expected formats:
    // - the.package.name.ClassName.MethodName
    // - the.package.name.ClassName.MethodName:50ms
    // - the.package.name.ClassName.MethodName(Jjava/lang/String;)V
    // - the.package.name.ClassName.MethodName(Jjava/lang/String;)V:50ms

    const char* last_dot = strrchr(s, '.');
    if (last_dot == NULL) {
        return Error("Unexpected format for tracing target");
    }
    std::string class_name(s, last_dot - s);

    if (class_name.find_first_of(":()") != std::string::npos) {
        // E.g. wrong signature
        // the.package.name.ClassName.MethodName(Jjava.lang.String;)V
        return Error("Unexpected format for tracing target");
    }

    // Replace all '.' in package name with '/'
    size_t dot_idx;
    size_t pos = 0;
    while ((dot_idx = class_name.find('.', pos)) != std::string::npos) {
        pos = dot_idx + 1;
        class_name[dot_idx] = '/';
    }

    // Latency
    Latency latency = default_latency;
    const char* colon = strchr(last_dot, ':');
    if (colon != NULL) {
        latency = Arguments::parseUnits(colon + 1, NANOS);
        if (latency < 0) {
            return Error("Invalid latency format in tracing target");
        }
    }

    std::string method;
    if (colon == NULL) {
        method = std::string(last_dot + 1);
    } else {
        method = std::string(last_dot + 1, colon - last_dot - 1);
    }

    targets[std::move(class_name)][std::move(method)] = latency;

    return Error::OK;
}

Error Instrument::setupTargetClassAndMethod(const Arguments& args) {
    _targets.clear();
    
    if (args._trace.empty()) {
        Error error = addTarget(_targets, args._event, NO_LATENCY);
        if (error) return error;
    } else {
        for (const char* s : args._trace) {
            Error error = addTarget(_targets, s, 0 /* default_latency */);
            if (error) return error;
        }
    }

    return Error::OK;
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
    for (int i = 0; i < class_count; i++) {
        char* signature;
        if (jvmti->GetClassSignature(classes[i], &signature, NULL) != 0) {
            continue;
        }
        size_t len;
        if (signature[0] != 'L' || signature[(len = strlen(signature)) - 1] != ';') {
            jvmti->Deallocate((unsigned char*)signature);
            continue;
        }

        for (const auto& target : _targets) {
            if (matchesPattern(signature + 1, len - 2, target.first)) {
                jboolean modifiable;
                if (
                    target.first[target.first.length() - 1] != '*' ||
                    // Some classes are not modifiable. With wildcard matching we skip
                    // them quietly; when the class is specifically selected by the user
                    // we let JVMTI fail loudly.
                    (jvmti->IsModifiableClass(classes[i], &modifiable) == 0 && modifiable)
                ) {
                    classes[matched_count++] = classes[i];
                }
                break;
            }
        }
        jvmti->Deallocate((unsigned char*)signature);
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

    if (name == NULL) {
        // Maybe we'll find a matching class name in the cpool?
        BytecodeRewriter rewriter(class_data, class_data_len, &_targets, nullptr);
        rewriter.rewrite(new_class_data, new_class_data_len);
        return;
    }

    size_t len = strlen(name);
    const MethodTargets* method_targets = findMethodTargets(&_targets, name, len);
    if (method_targets != nullptr) {
        BytecodeRewriter rewriter(class_data, class_data_len, nullptr, method_targets);
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

void JNICALL Instrument::recordExit0(JNIEnv* jni, jobject unused, jlong startTimeNs) {
    if (!_enabled) return;

    if (shouldRecordSample()) {
        u64 now_ticks = TSC::ticks();
        u64 duration_ns = OS::nanotime() - (u64) startTimeNs;
        u64 duration_ticks = (u64) ((double) duration_ns * TSC::frequency() / NANOTIME_FREQ);
        MethodTraceEvent event(now_ticks - duration_ticks, duration_ticks);
        Profiler::instance()->recordSample(NULL, duration_ns, METHOD_TRACE, &event);
    }
}
