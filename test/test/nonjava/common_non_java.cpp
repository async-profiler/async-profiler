/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common_non_java.h"

asprof_init_t CommonNonJava::asprof_init;
asprof_execute_t CommonNonJava::asprof_execute;
asprof_error_str_t CommonNonJava::asprof_error_str;

JavaVM* CommonNonJava::jvm;
JNIEnv* CommonNonJava::env;

void* CommonNonJava::jvmLib;

void CommonNonJava::outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

void CommonNonJava::loadProfiler() {  
    #ifdef __linux__
    void* lib = dlopen("build/lib/libasyncProfiler.so", RTLD_NOW | RTLD_GLOBAL);
    #else
    void* lib = dlopen("build/lib/libasyncProfiler.dylib", RTLD_NOW | RTLD_GLOBAL);
    #endif
    if (lib == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    asprof_error_str = (asprof_error_str_t)dlsym(lib, "asprof_error_str");

    asprof_init();
}

void CommonNonJava::startProfiler() {
    asprof_error_t err = asprof_execute("start,event=cpu,interval=1ms,cstack=dwarf", outputCallback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }
}

void CommonNonJava::stopProfiler(char* outputFile) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "stop,file=%s", outputFile);
    asprof_error_t err = asprof_execute(cmd, outputCallback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }
}

void CommonNonJava::loadJvmLib() {
    char* java_home = getenv("JAVA_HOME");
    if (java_home == NULL) {
        fprintf(stderr, "JAVA_HOME is not set\n");
        exit(1);
    }
    char libPath[4096];
    #ifdef __linux__
    snprintf(libPath, sizeof(libPath), "%s/%s", java_home, "lib/server/libjvm.so");
    #else
    snprintf(libPath, sizeof(libPath), "%s/%s", java_home, "lib/server/libjvm.dylib");
    #endif
    jvmLib = dlopen(libPath, RTLD_LOCAL | RTLD_NOW);
}

void CommonNonJava::startJvm() {
    // Start JVM
    JavaVMInitArgs vm_args;
    JavaVMOption options[1];
    int pid = getpid();

    options[0].optionString = const_cast<char*>("-Djava.class.path=build/test");

    // Configure JVM
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = true;


    CreateJvm createJvm = (CreateJvm)dlsym(jvmLib, "JNI_CreateJavaVM");

    // Create the JVM
    jint rc = createJvm(&jvm, (void**)&env, &vm_args);
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to create JVM\n");
        exit(1);
    }
}

void CommonNonJava::executeJvmTask() {
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
}

void CommonNonJava::stopJvm() {
    jint rc = jvm->DestroyJavaVM();
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to destroy JVM\n");
        exit(1);
    }
}