/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dlfcn.h>
#include <jni.h>
#include <unistd.h>
#include <iostream>
#include <limits.h>
#include "asprof.h"
#include <pthread.h>

// detect arch for java 8
#if defined(__x86_64__) || defined(_M_X64)
#  define JAVA8_ARCH_PATH "amd64/"
#elif defined(__i386) || defined(_M_IX86)
#  define JAVA8_ARCH_PATH "i386/"
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define JAVA8_ARCH_PATH "aarch64/"
#elif defined(__arm__) || defined(_M_ARM)
#  define JAVA8_ARCH_PATH "arm/"
#else
#  define JAVA8_ARCH_PATH "/"
#endif
 
#ifdef __linux__
const char profiler_lib_path[] = "build/lib/libasyncProfiler.so";
const char jvm_lib_path[] = "lib/server/libjvm.so";
const char jvm8_lib_path[] = "lib/" JAVA8_ARCH_PATH "server/libjvm.so";
#else
const char profiler_lib_path[] = "build/lib/libasyncProfiler.dylib";
const char jvm_lib_path[] = "lib/server/libjvm.dylib";
const char jvm8_lib_path[] = "lib/server/libjvm.dylib";
#endif

typedef jint (*CreateJvm)(JavaVM **, void **, void *);

asprof_init_t _asprof_init;
asprof_execute_t _asprof_execute;
asprof_error_str_t _asprof_error_str;

JavaVM* _jvm;
JNIEnv* _env;

void* _jvm_lib;

jint java_version = 0;

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

void loadProfiler() {  
    void* lib = dlopen(profiler_lib_path, RTLD_NOW | RTLD_GLOBAL);
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
    // Get Java home
    char* java_home = getenv("TEST_JAVA_HOME");
    if (java_home == NULL) {
        std::cerr << "TEST_JAVA_HOME is not set" << std::endl;
        exit(1);
    }
    // Get Java version
    char* version = getenv("TEST_JAVA_VERSION");
    if (version == NULL) {
        std::cerr << "TEST_JAVA_VERSION is not set" << std::endl;
        exit(1);
    }
    // Java 8 or higher
    java_version = std::stoi(version);
    if (java_version < 8) {
        std::cerr << "Unsupported Java version: " << version << std::endl;
        exit(1);
    }

    char lib_path[PATH_MAX];
    snprintf(lib_path, sizeof(lib_path), "%s/%s", java_home, java_version == 8 ? jvm8_lib_path : jvm_lib_path);
    _jvm_lib = dlopen(lib_path, RTLD_LOCAL | RTLD_NOW);
    if (_jvm_lib == NULL) {
        std::cerr << "Unable to find: " << lib_path << ", Error: " << dlerror() << std::endl;
        exit(1);
    }
}

void startJvm() {
    // Start JVM
    JavaVMInitArgs vm_args;
    JavaVMOption options[1];
    int pid = getpid();

    options[0].optionString = const_cast<char*>("-Djava.class.path=build/test");
    //options[1].optionString = const_cast<char*>("-Xcheck:jni");

    // Configure JVM
    vm_args.version = JNI_VERSION_1_6;
    vm_args.nOptions = 1;
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

    for (int i = 0; i < 300; ++i) {
        jdouble result = _env->CallStaticDoubleMethod(customClass, cpuHeavyTask);
        if (_env->ExceptionCheck()) {
            _env->ExceptionDescribe();
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
    validateArgsCount(argc, 4, "Test requires 3 arguments");

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

void* jvmThreadWrapper(void* arg) {
    loadJvmLib();

    startJvm();

    while(1) executeJvmTask();

    return NULL;
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM on  a different thread
4. Start the profiler
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task

Explaination:
The JVM is loaded and started before the profiling session is started so it's attached correctly at the session start
*/
void testFlow4(int argc, char** argv) {
    validateArgsCount(argc, 3, "Minimum Arguments is 2");

    loadProfiler();

    pthread_t thread;
    pthread_create(&thread, NULL, jvmThreadWrapper, NULL);
   
    // busy wait for JVM to start on the thread
    for (int i = 0; i < 10; i++) for (int i = 0; i < 1000000000; ++i) {}

    startProfiler();

    // busy wait for profiler to sample JVM
    for (int i = 0; i < 10; i++) for (int i = 0; i < 1000000000; ++i) {}

    stopProfiler(argv[2]);
}

int main(int argc, char** argv) {
    validateArgsCount(argc, 3, "Minimum Arguments is 2");

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
        case '4':
            testFlow4(argc, argv);
            break;
        default:
            std::cerr << "Unknown flow: " << flow[0] << std::endl;
            exit(1);
    }
    
    return 0;
}
