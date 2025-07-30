/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LOOKUP_H
#define _LOOKUP_H

#include <assert.h>
#include <unordered_map>
#include <jvmti.h>
#include "arguments.h"
#include "vmEntry.h"

class Dictionary;
class Index;

class MethodInfo {
  public:
    bool _mark = false;
    u32 _key = 0;
    u32 _class = 0;
    u32 _name = 0;
    u32 _sig = 0;
    jint _modifiers = 0;
    jint _line_number_table_size = 0;
    jvmtiLineNumberEntry* _line_number_table = nullptr;
    FrameTypeId _type;

    jint getLineNumber(jint bci);
};

class MethodMap : public std::unordered_map<jmethodID, MethodInfo> {
  public:
    MethodMap() = default;
    ~MethodMap();

    size_t usedMemory();
};

class Lookup {
  public:
    MethodMap* _method_map;
    // Dictionary is thread-safe and can be safely shared, as opposed to Index
    Dictionary* _classes;
    Index* _packages;
    Index* _symbols;

    Lookup(MethodMap* method_map, Dictionary* classes, Index* packages, Index* symbols, Output output) :
        _method_map(method_map),
        _classes(classes),
        _packages(packages),
        _symbols(symbols),
        _output_type(output),
        _jni(VM::jni()) {
        assert(_packages != nullptr || output != OUTPUT_JFR);
    }

    MethodInfo* resolveMethod(ASGCT_CallFrame& frame);
    u32 getPackage(const char* class_name);

  private:
    JNIEnv* _jni;
    Output _output_type;

    void fillNativeMethodInfo(MethodInfo* mi, const char* name, const char* lib_name);
    bool fillJavaMethodInfo(MethodInfo* mi, jmethodID method, bool first_time);
    void fillJavaClassInfo(MethodInfo* mi, u32 class_id);
};

#endif // _LOOKUP_H
