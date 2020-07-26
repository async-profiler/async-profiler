/*
 * Copyright 2019 Andrei Pangin
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
    const char* name() {
        return "instrument";
    }

    const char* units() {
        return "calls";
    }

    CStack cstack() {
        return CSTACK_NO;
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

    static void recordSample();
};

#endif // _INSTRUMENT_H
