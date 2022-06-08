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

    ClassMap _class_names;
    std::vector<Matcher> _include;
    std::vector<Matcher> _exclude;
    char _buf[800];  // must be large enough for class name + method name + method signature
    int _style;
    unsigned char _cache_epoch;
    unsigned char _cache_max_age;
    Mutex& _thread_names_lock;
    ThreadMap& _thread_names;
    locale_t _saved_locale;

    void buildFilter(std::vector<Matcher>& vector, const char* base, int offset);
    char* truncate(char* name, int max_length);
    const char* decodeNativeSymbol(const char* name);
    const char* typeSuffix(FrameTypeId type);
    char* javaMethodName(jmethodID method);
    char* javaClassName(const char* symbol, int length, int style);

  public:
    FrameName(Arguments& args, int style, int epoch, Mutex& thread_names_lock, ThreadMap& thread_names);
    ~FrameName();

    const char* name(ASGCT_CallFrame& frame, bool for_matching = false);

    bool hasIncludeList() { return !_include.empty(); }
    bool hasExcludeList() { return !_exclude.empty(); }

    bool include(const char* frame_name);
    bool exclude(const char* frame_name);
};

#endif // _FRAMENAME_H
