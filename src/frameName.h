/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FRAMENAME_H
#define _FRAMENAME_H

#include <jvmti.h>
#include <locale.h>
#include <map>
#include <vector>
#include <string>
#include "arguments.h"
#include "codeCache.h"
#include "mutex.h"
#include "vmEntry.h"

#ifdef __APPLE__
#  include <xlocale.h>
#endif


typedef std::map<jmethodID, std::string> JMethodCache;
typedef std::map<int, std::string> ThreadMap;
typedef std::map<unsigned int, const char*> ClassMap;


enum MatchType {
  MATCH_EQUALS,
  MATCH_CONTAINS,
  MATCH_STARTS_WITH,
  MATCH_ENDS_WITH
};


class Matcher {
  private:
    MatchType _type;
    char* _pattern;
    int _len;

  public:
    Matcher(const char* pattern);
    ~Matcher();

    Matcher(const Matcher& m);
    Matcher& operator=(const Matcher& m);

    bool matches(const char* s);
};

class FrameInfo {
  private:
    const char* _name;
    // E.g. non demangled name
    const char* _system_name;
    const char* _file_name;
    const CodeCache* _cc;
    const u64 _address;
    const u64 _line;

  public:
    FrameInfo(const char* name, const char* system_name, const char* file_name, const CodeCache* cc, u64 address, u64 line) :
      _name(name), _system_name(system_name), _file_name(file_name), _cc(cc), _address(address), _line(line) {}

    FrameInfo(const char* name, const char* system_name, const CodeCache* cc) :
      FrameInfo(name, system_name, nullptr, cc, 0, 0) {}

    FrameInfo(const char* name) : FrameInfo(name, nullptr, nullptr, nullptr, 0, 0) {}

    const char* get_name() const {
      return _name;
    }

    const char* get_system_name() const {
      return _system_name;
    }

    const char* get_file_name() const {
      return _file_name;
    }

    const CodeCache* get_lib() const {
      return _cc;
    }

    u64 get_address() const {
      return _address;
    }

    u64 get_line() const {
      return _line;
    }
};

class FrameName {
  private:
    static JMethodCache _cache;

    JNIEnv* _jni;
    ClassMap _class_names;
    std::vector<Matcher> _include;
    std::vector<Matcher> _exclude;
    std::string _str;
    int _style;
    unsigned char _cache_epoch;
    unsigned char _cache_max_age;
    Mutex& _thread_names_lock;
    ThreadMap& _thread_names;
    locale_t _saved_locale;

    void buildFilter(std::vector<Matcher>& vector, const char* base, int offset);
    const char* decodeNativeSymbol(const char* name, const CodeCache* cc);
    const char* typeSuffix(FrameTypeId type);
    void javaMethodName(jmethodID method);
    void javaClassName(const char* symbol, size_t length, int style);

  public:
    FrameName(Arguments& args, int style, int epoch, Mutex& thread_names_lock, ThreadMap& thread_names);
    ~FrameName();

    FrameInfo frameInfo(ASGCT_CallFrame& frame, bool for_matching = false);
    const char* name(ASGCT_CallFrame& frame, bool for_matching = false);
    FrameTypeId type(ASGCT_CallFrame& frame);

    bool hasIncludeList() { return !_include.empty(); }
    bool hasExcludeList() { return !_exclude.empty(); }

    bool include(const char* frame_name);
    bool exclude(const char* frame_name);
};

#endif // _FRAMENAME_H
