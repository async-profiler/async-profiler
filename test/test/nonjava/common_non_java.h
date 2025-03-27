/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _COMMON_NONE_JAVA_TEST_H
#define _COMMON_NONE_JAVA_TEST_H

#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>
#include <iostream>
#include "asprof.h"

typedef jint (*CreateJvm)(JavaVM **, void **, void *);

class CommonNonJava {
  public:
    static asprof_init_t asprof_init;
    static asprof_execute_t asprof_execute;
    static asprof_error_str_t asprof_error_str;

    static JavaVM* jvm;
    static JNIEnv* env;

    static void* jvmLib;

    static void outputCallback(const char* buffer, size_t size);
    static void loadProfiler();
    static void startProfiler();
    static void stopProfiler(char* outputFile);
    static void loadJvmLib();
    static void startJvm();
    static void executeJvmTask();
    static void stopJvm();

};

#endif // _COMMON_NONE_JAVA_TEST_H