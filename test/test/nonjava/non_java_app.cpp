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
const char profiler_lib_path[] = "build/lib/libasyncProfiler.so";
const char jvm_lib_path[] = "lib/server/libjvm.so";
#else
const char profiler_lib_path[] = "build/lib/libasyncProfiler.dylib";
const char jvm_lib_path[] = "lib/server/libjvm.dylib";
#endif

typedef jint (*CreateJvm)(JavaVM **, void **, void *);

asprof_init_t _asprof_init;
asprof_execute_t _asprof_execute;
asprof_error_str_t _asprof_error_str;

JavaVM* _jvm;
JNIEnv* _env;

void* _jvm_lib;

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

void loadProfiler() {  
    void* lib = dlopen("build/lib/libasyncProfiler.so", RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL) {
        std::cerr << dlerror() << std::endl;
        exit(1);
    }

    _asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    _asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    _asprof_error_str = (asprof_error_str_t)dlsym(lib, "asprof_error_str");

    _asprof_init();
}

void startProfiler() {
    asprof_error_t err = _asprof_execute("start,event=cpu,interval=1ms,cstack=dwarf", outputCallback);
    if (err != NULL) {
        std::cerr << _asprof_error_str(err) << std::endl;
        exit(1);
    }
}

void stopProfiler(char* outputFile) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "stop,file=%s", outputFile);
    asprof_error_t err = _asprof_execute(cmd, outputCallback);
    if (err != NULL) {
        std::cerr << _asprof_error_str(err) << std::endl;
        exit(1);
    }
}

void loadJvmLib() {
    char* java_home = getenv("JAVA_HOME");
    if (java_home == NULL) {
        std::cerr << "JAVA_HOME is not set" << std::endl;
        exit(1);
    }
    char lib_path[4096];
    snprintf(lib_path, sizeof(lib_path), "%s/%s", java_home, jvm_lib_path);
    _jvm_lib = dlopen(lib_path, RTLD_LOCAL | RTLD_NOW);
    if (_jvm_lib == NULL) {
        std::cerr << "Unable to find: " << lib_path << ", Error: " << dlerror() << std::endl;
        exit(1);
    }
}

void startJvm() {
    // Start JVM
    JavaVMInitArgs vm_args;
    JavaVMOption options[2];
    int pid = getpid();

    options[0].optionString = const_cast<char*>("-Djava.class.path=build/test");
    options[1].optionString = const_cast<char*>("-Xcheck:jni");

    // Configure JVM
    vm_args.version = JNI_VERSION_10;
    vm_args.nOptions = 2;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = true;

    CreateJvm createJvm = (CreateJvm)dlsym(_jvm_lib, "JNI_CreateJavaVM");
    if (createJvm == NULL) {
        std::cerr << "Unable to find: JNI_CreateJavaVM" << std::endl;
        exit(1);
    }

    // Create the JVM
    jint rc = createJvm(&_jvm, (void**)&_env, &vm_args);
    if (rc != JNI_OK) {
        std::cerr << "Failed to create JVM" << std::endl;
        exit(1);
    }
}

void executeJvmTask() {
    jclass customClass = _env->FindClass("JavaClass");
    if (customClass == nullptr) {
        std::cerr << "Can't find JavaClass" << std::endl;
        exit(1);
    }

    jmethodID cpuHeavyTask = _env->GetStaticMethodID(customClass, "cpuHeavyTask", "()D");
    if (cpuHeavyTask == nullptr) {
        std::cerr << "Can't find cpuHeavyTask" << std::endl;
        exit(1);
    }

    for (int i = 0; i < 100; ++i) {
        jdouble result = _env->CallStaticDoubleMethod(customClass, cpuHeavyTask);
        if (_env->ExceptionCheck()) {
            jthrowable exception = _env->ExceptionOccurred();
            _env->ExceptionDescribe();
            _env->ExceptionClear();
            std::cerr << "Exception in cpuHeavyTask" << std::endl;
            exit(1);
        }
        std::cout << "Result: " << result << std::endl;
    }
    _env->DeleteLocalRef(customClass);
}

void stopJvm() {
    jint rc = _jvm->DestroyJavaVM();
    if (rc != JNI_OK) {
        std::cerr << "Failed to destroy JVM" << std::endl;
        exit(1);
    }
}

void validateArgsCount(int argc, int exepected, std::string context) {
    if (argc < exepected) {
        std::cerr << "Too few arguments: " << context << std::endl;
        exit(1);
    }  
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM
4. Start the profiler
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task

Explaination:
The JVM is loaded and started before the profiling session is started so it's attached correctly at the session start
*/
void testFlow1(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startJvm();

    startProfiler();

    executeJvmTask();

    stopProfiler(argv[2]);

    stopJvm();
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the profiler
4. Start the JVM
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler will not be able to sample the JVM stacks correctly

Explaination:
The JVM is started after the profiling session is started so it's not attached correctly at the session start
*/
void testFlow2(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startProfiler();

    startJvm();

    executeJvmTask();

    stopProfiler(argv[2]);

    stopJvm();
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the profiler
4. Start the JVM
5. Execute the JVM task
6. Stop the profiler
7. Start the profiler
8. Execute the JVM task
9. Stop the profiler
10. Stop the JVM

Expected output:
The profiler will not be able to sample the JVM stacks correctly on the first session
But will be able to sample the JVM stacks correctly on the second session

Explaination:
The JVM is started after the profiling session is started so it's not attached correctly at the session start
However the second profiling session is started after the JVM is started so it's attached correctly at the session start
*/
void testFlow3(int argc, char** argv) {
    validateArgsCount(argc, 4, "Test requires 4 arguments");

    loadProfiler();

    loadJvmLib();

    startProfiler();

    startJvm();

    executeJvmTask();

    stopProfiler(argv[2]);

    startProfiler();

    executeJvmTask();

    stopProfiler(argv[3]);

    stopJvm();
}

int main(int argc, char** argv) {
    validateArgsCount(argc, 3, "Minimum Arguments is 3");

    // Check which test to run
    char* flow = argv[1];
    switch (flow[0]) {
        case '1':
            testFlow1(argc, argv);
            break;
        case '2': 
            testFlow2(argc, argv);
            break;
        case '3': 
            testFlow3(argc, argv);
            break;
        default:
            std::cerr << "Unknown flow: " << flow[0] << std::endl;
            exit(1);
    }
    
    return 0;
}
