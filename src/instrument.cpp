/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include "arch.h"
#include "incbin.h"
#include "profiler.h"
#include "tsc.h"
#include "vmEntry.h"
#include "instrument.h"


INCLUDE_HELPER_CLASS(INSTRUMENT_NAME, INSTRUMENT_CLASS, "one/profiler/Instrument")


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
};

enum Scope {
    SCOPE_CLASS,
    SCOPE_FIELD,
    SCOPE_METHOD,
    SCOPE_REWRITE_METHOD,
    SCOPE_REWRITE_CODE
};

enum PatchConstants {
    EXTRA_CONSTANTS = 6,
    EXTRA_BYTECODES = 4,
    EXTRA_STACKMAPS = 1
};


class BytecodeRewriter {
  private:
    const u8* _src;
    const u8* _src_limit;

    u8* _dst;
    int _dst_len;
    int _dst_capacity;

    Constant** _cpool;
    u16 _cpool_len;

    const char* _target_class;
    u16 _target_class_len;
    const char* _target_method;
    u16 _target_method_len;
    const char* _target_signature;
    u16 _target_signature_len;

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

    void rewriteCode();
    void rewriteBytecodeTable(int data_len);
    void rewriteStackMapTable();
    void rewriteVerificationTypeInfo();
    void rewriteAttributes(Scope scope);
    void rewriteMembers(Scope scope);
    bool rewriteClass();

  public:
    BytecodeRewriter(const u8* class_data, int class_data_len, const char* target_class) :
        _src(class_data),
        _src_limit(class_data + class_data_len),
        _dst(NULL),
        _dst_len(0),
        _dst_capacity(class_data_len + 400),
        _cpool(NULL) {

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


void BytecodeRewriter::rewriteCode() {
    u32 attribute_length = get32();
    put32(attribute_length);

    int code_begin = _dst_len;

    u16 max_stack = get16();
    put16(max_stack);

    u16 max_locals = get16();
    put16(max_locals);

    u32 code_length = get32();
    put32(code_length + EXTRA_BYTECODES);

    // invokestatic "one/profiler/Instrument.recordSample()V"
    // nop ensures that tableswitch/lookupswitch needs no realignment
    put8(0xb8);
    put16(_cpool_len);
    put8(0);
    // The rest of the code is unchanged
    put(get(code_length), code_length);

    u16 exception_table_length = get16();
    put16(exception_table_length);

    for (int i = 0; i < exception_table_length; i++) {
        u16 start_pc = get16();
        u16 end_pc = get16();
        u16 handler_pc = get16();
        u16 catch_type = get16();
        put16(EXTRA_BYTECODES + start_pc);
        put16(EXTRA_BYTECODES + end_pc);
        put16(EXTRA_BYTECODES + handler_pc);
        put16(catch_type);
    }

    rewriteAttributes(SCOPE_REWRITE_CODE);

    // Patch attribute length
    *(u32*)(_dst + code_begin - 4) = htonl(_dst_len - code_begin);
}

void BytecodeRewriter::rewriteBytecodeTable(int data_len) {
    u32 attribute_length = get32();
    put32(attribute_length);

    u16 table_length = get16();
    put16(table_length);

    for (int i = 0; i < table_length; i++) {
        u16 start_pc = get16();
        put16(EXTRA_BYTECODES + start_pc);

        put(get(data_len), data_len);
    }
}

void BytecodeRewriter::rewriteStackMapTable() {
    u32 attribute_length = get32();
    put32(attribute_length + EXTRA_STACKMAPS);

    u16 number_of_entries = get16();
    put16(number_of_entries + EXTRA_STACKMAPS);

    // Prepend same_frame
    put8(EXTRA_BYTECODES - 1);

    for (int i = 0; i < number_of_entries; i++) {
        u8 frame_type = get8();
        put8(frame_type);

        if (frame_type <= 63) {
            // same_frame
        } else if (frame_type <= 127) {
            // same_locals_1_stack_item_frame
            rewriteVerificationTypeInfo();
        } else if (frame_type == 247) {
            // same_locals_1_stack_item_frame_extended
            put16(get16());
            rewriteVerificationTypeInfo();
        } else if (frame_type <= 251) {
            // chop_frame or same_frame_extended
            put16(get16());
        } else if (frame_type <= 254) {
            // append_frame
            put16(get16());
            for (int j = 0; j < frame_type - 251; j++) {
                rewriteVerificationTypeInfo();
            }
        } else {
            // full_frame
            put16(get16());
            u16 number_of_locals = get16();
            put16(number_of_locals);
            for (int j = 0; j < number_of_locals; j++) {
                rewriteVerificationTypeInfo();
            }
            u16 number_of_stack_items = get16();
            put16(number_of_stack_items);
            for (int j = 0; j < number_of_stack_items; j++) {
                rewriteVerificationTypeInfo();
            }
        }
    }
}

void BytecodeRewriter::rewriteVerificationTypeInfo() {
    u8 tag = get8();
    put8(tag);
    if (tag >= 7) {
        // Adjust ITEM_Uninitialized offset
        put16(tag == 8 ? EXTRA_BYTECODES + get16() : get16());
    }
}

void BytecodeRewriter::rewriteAttributes(Scope scope) {
    u16 attributes_count = get16();
    put16(attributes_count);

    for (int i = 0; i < attributes_count; i++) {
        u16 attribute_name_index = get16();
        put16(attribute_name_index);

        Constant* attribute_name = _cpool[attribute_name_index];
        if (scope == SCOPE_REWRITE_METHOD && attribute_name->equals("Code", 4)) {
            rewriteCode();
            continue;
        } else if (scope == SCOPE_REWRITE_CODE) {
            if (attribute_name->equals("LineNumberTable", 15)) {
                rewriteBytecodeTable(2);
                continue;
            } else if (attribute_name->equals("LocalVariableTable", 18) ||
                       attribute_name->equals("LocalVariableTypeTable", 22)) {
                rewriteBytecodeTable(8);
                continue;
            } else if (attribute_name->equals("StackMapTable", 13)) {
                rewriteStackMapTable();
                continue;
            }
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

        rewriteAttributes(need_rewrite ? SCOPE_REWRITE_METHOD : SCOPE_METHOD);
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
    putConstant("recordSample");
    putConstant("()V");

    u16 access_flags = get16();
    put16(access_flags);

    u16 this_class = get16();
    put16(this_class);

    u16 class_name_index = _cpool[this_class]->info();
    if (!_cpool[class_name_index]->equals(_target_class, _target_class_len)) {
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
volatile u64 Instrument::_calls;
volatile bool Instrument::_running;

Error Instrument::check(Arguments& args) {
    if (!_instrument_class_loaded) {
        if (!VM::loaded()) {
            return Error("Profiling event is not supported with non-Java processes");
        }

        JNIEnv* jni = VM::jni();
        const JNINativeMethod native_method = {(char*)"recordSample", (char*)"()V", (void*)recordSample};

        jclass cls = jni->DefineClass(INSTRUMENT_NAME, NULL, (const jbyte*)INSTRUMENT_CLASS, INCBIN_SIZEOF(INSTRUMENT_CLASS));
        if (cls == NULL || jni->RegisterNatives(cls, &native_method, 1) != 0) {
            jni->ExceptionDescribe();
            return Error("Could not load Instrument class");
        }

        _instrument_class_loaded = true;
    }

    return Error::OK;
}

Error Instrument::start(Arguments& args) {
    Error error = check(args);
    if (error) {
        return error;
    }

    if (args._interval < 0) {
        return Error("interval must be positive");
    }

    setupTargetClassAndMethod(args._event);
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
    if (jvmti->GetLoadedClasses(&class_count, &classes) != 0) {
        return;
    }

    jint matched_count = 0;
    size_t len = strlen(_target_class);
    for (int i = 0; i < class_count; i++) {
        char* signature;
        if (jvmti->GetClassSignature(classes[i], &signature, NULL) == 0) {
            if (signature[0] == 'L' && strncmp(signature + 1, _target_class, len) == 0 && signature[len + 1] == ';') {
                classes[matched_count++] = classes[i];
            }
            jvmti->Deallocate((unsigned char*)signature);
        }
    }

    if (matched_count > 0) {
        jvmti->RetransformClasses(matched_count, classes);
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

    if (name == NULL || strcmp(name, _target_class) == 0) {
        BytecodeRewriter rewriter(class_data, class_data_len, _target_class);
        rewriter.rewrite(new_class_data, new_class_data_len);
    }
}

void JNICALL Instrument::recordSample(JNIEnv* jni, jobject unused) {
    if (!_enabled) return;

    if (_interval <= 1 || ((atomicInc(_calls) + 1) % _interval) == 0) {
        ExecutionEvent event(TSC::ticks());
        Profiler::instance()->recordSample(NULL, _interval, INSTRUMENTED_METHOD, &event);
    }
}
