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
    const char* decodeNativeSymbol(const char* name);
    const char* typeSuffix(FrameTypeId type);
    void javaMethodName(jmethodID method);
    void javaClassName(const char* symbol, size_t length, int style);

  public:
    FrameName(Arguments& args, int style, int epoch, Mutex& thread_names_lock, ThreadMap& thread_names);
    ~FrameName();

    const char* name(ASGCT_CallFrame& frame, bool for_matching = false);
    FrameTypeId type(ASGCT_CallFrame& frame);

    bool hasIncludeList() { return !_include.empty(); }
    bool hasExcludeList() { return !_exclude.empty(); }

    bool include(const char* frame_name);
    bool exclude(const char* frame_name);
};

#endif // _FRAMENAME_H
