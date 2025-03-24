/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>
#include <iostream>
#include "asprof.h"

#ifdef __linux__
#  define PROFILER_PATH "build/lib/libasyncProfiler.so"
#else
#  define PROFILER_PATH "build/lib/libasyncProfiler.dylib"
#endif

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Too few arguments");
        exit(1);
    }

    // Start JVMpwd
    JavaVM* jvm;
    JNIEnv* env;
    JavaVMInitArgs vm_args;
    JavaVMOption options[1];

    int pid = getpid();

    options[0].optionString = const_cast<char*>("-Djava.class.path=build/test");

    // Configure JVM
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = true;

    // Create the JVM
    jint rc = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to create JVM\n");
        exit(1);
    }

    // ---------------------------------------------------------------------------------------------

    // Start Async profiler
    void* lib = dlopen(PROFILER_PATH, RTLD_NOW);
    if (lib == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    asprof_init_t asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    asprof_execute_t asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    asprof_error_str_t asprof_error_str = (asprof_error_str_t)dlsym(lib, "asprof_error_str");

    if (asprof_init == NULL || asprof_execute == NULL || asprof_error_str == NULL) {
        printf("%s\n", dlerror());
        dlclose(lib);
        exit(1);
    }

    asprof_init();

    asprof_error_t err = asprof_execute("start,event=cpu,interval=1ms,cstack=dwarf", outputCallback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }

    // --------------------------------------------------------------------------------------------

    for (int i = 0; i < 100; ++i) {
        jclass customClass = env->FindClass("JavaClass");
        if (customClass == nullptr) {
            fprintf(stderr, "Can't find JavaClass\n");
            exit(1);
        }
        jmethodID constructor = env->GetMethodID(customClass, "<init>", "()V");
        jobject customObject = env->NewObject(customClass, constructor);
        jmethodID cpuHeavyTask = env->GetMethodID(customClass, "cpuHeavyTask", "()V");
        if (cpuHeavyTask == nullptr) {
            fprintf(stderr, "Can't find cpuHeavyTask");
            exit(1);
        }
        env->CallVoidMethod(customObject, cpuHeavyTask);

        env->DeleteLocalRef(customObject);
        env->DeleteLocalRef(customClass);
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "stop,file=%s", argv[1]);
    err = asprof_execute(cmd, outputCallback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }

    rc = jvm->DestroyJavaVM();
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to destroy JVM\n");
        exit(1);
    }
    return 0;
}
