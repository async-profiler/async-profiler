/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INSTRUMENT_H
#define _INSTRUMENT_H

#include <jvmti.h>
#include "engine.h"


class Instrument : public Engine {
  private:
    static char* _target_class;
    static bool _instrument_class_loaded;
    static u64 _interval;
    static volatile u64 _calls;
    static volatile bool _running;

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

    void setupTargetClassAndMethod(const char* event);

    void retransformMatchedClasses(jvmtiEnv* jvmti);

    static void JNICALL ClassFileLoadHook(jvmtiEnv* jvmti, JNIEnv* jni,
                                          jclass class_being_redefined, jobject loader,
                                          const char* name, jobject protection_domain,
                                          jint class_data_len, const u8* class_data,
                                          jint* new_class_data_len, u8** new_class_data);

    static void JNICALL recordSample(JNIEnv* jni, jobject unused);
};

#endif // _INSTRUMENT_H
