/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INSTRUMENT_H
#define _INSTRUMENT_H

#include <jvmti.h>
#include <map>
#include <string>
#include <vector>
#include "arch.h"
#include "engine.h"

typedef std::string ClassName;
typedef std::string MethodName;

class MethodTarget {
  public:
    // Fictitious latency which disables method tracing and falls back to
    // JVM method instrumentaton.
    static const long NO_LATENCY = -1;

    std::string signature;
    long latency;

    MethodTarget(std::string&& signature, long latency) :
        signature(std::move(signature)),
        latency(latency) {}

    MethodTarget(const MethodTarget&) = delete;
    MethodTarget& operator=(const MethodTarget&) = delete;
    MethodTarget(MethodTarget&& other)
        : signature(std::move(other.signature)), latency(other.latency) {}
};

typedef std::map<MethodName, std::vector<MethodTarget>> MethodTargets;
typedef std::map<ClassName, MethodTargets> Targets;

const MethodTargets EMPTY_METHOD_TARGETS;

class Instrument : public Engine {
  private:
    static Targets _targets;
    static bool _instrument_class_loaded;
    static u64 _interval;
    static volatile u64 _calls;
    static volatile bool _running;

    static bool shouldRecordSample() {
        return _interval <= 1 || ((atomicInc(_calls) + 1) % _interval) == 0;
    }

  public:
    const char* type() {
        return "instrument";
    }

    const char* title() {
        return "Java method profile";
    }

    const char* units() {
        return "calls";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    Error setupTargetClassAndMethod(const Arguments& args);

    void retransformMatchedClasses(jvmtiEnv* jvmti);

    static void JNICALL ClassFileLoadHook(jvmtiEnv* jvmti, JNIEnv* jni,
                                          jclass class_being_redefined, jobject loader,
                                          const char* name, jobject protection_domain,
                                          jint class_data_len, const u8* class_data,
                                          jint* new_class_data_len, u8** new_class_data);

    static void JNICALL recordEntry(JNIEnv* jni, jobject unused);
    static void JNICALL recordExit0(JNIEnv* jni, jobject unused, jlong startTimeNs);
};

#endif // _INSTRUMENT_H
