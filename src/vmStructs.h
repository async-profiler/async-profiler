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

#ifndef _VMSTRUCTS_H
#define _VMSTRUCTS_H

#include <jvmti.h>
#include <stdint.h>
#include "codeCache.h"


class VMStructs {
  protected:
    static int _klass_name_offset;
    static int _symbol_length_offset;
    static int _symbol_length_and_refcount_offset;
    static int _symbol_body_offset;
    static int _class_klass_offset;
    static int _thread_osthread_offset;
    static int _osthread_id_offset;
    static bool _has_perm_gen;
    static jfieldID _eetop;

    const char* at(int offset) {
        return (const char*)this + offset;
    }

  public:
    static void init(NativeLib* libjvm);

    static bool available() {
        return _klass_name_offset >= 0
            && (_symbol_length_offset >= 0 || _symbol_length_and_refcount_offset >= 0)
            && _symbol_body_offset >= 0
            && _class_klass_offset >= 0;
    }

    static bool hasPermGen() {
        return _has_perm_gen;
    }
};


class VMSymbol : VMStructs {
  public:
    unsigned short length() {
        if (_symbol_length_offset >= 0) {
          return *(unsigned short*) at(_symbol_length_offset);
        } else {
          int length_and_refcount = *(unsigned int*) at(_symbol_length_and_refcount_offset);
          return (length_and_refcount >> 16) & 0xffff;
        }
    }

    const char* body() {
        return at(_symbol_body_offset);
    }
};

class VMKlass : VMStructs {
  public:
    static VMKlass* fromHandle(uintptr_t handle) {
        if (_has_perm_gen) {
            // On JDK 7 KlassHandle is a pointer to klassOop, hence one more indirection
            return (VMKlass*)(*(uintptr_t**)handle + 2);
        } else {
            return (VMKlass*)handle;
        }
    }

    VMSymbol* name() {
        return *(VMSymbol**) at(_klass_name_offset);
    }
};

class java_lang_Class : VMStructs {
  public:
    VMKlass* klass() {
        return *(VMKlass**) at(_class_klass_offset);
    }
};

class VMThread : VMStructs {
  public:
    static bool available() {
        return _eetop != NULL;
    }

    static VMThread* fromJavaThread(JNIEnv* env, jthread thread) {
        return (VMThread*)(uintptr_t)env->GetLongField(thread, _eetop);
    }

    int osThreadId() {
        const char* osthread = *(const char**) at(_thread_osthread_offset);
        return *(int*)(osthread + _osthread_id_offset);
    }
};

#endif // _VMSTRUCTS_H
