/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <dlfcn.h>
#include <jni.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "asprof.h"

#ifdef __linux__
const char profiler_lib_path[] = "build/lib/libasyncProfiler.so";
const char jvm_lib_path[] = "server/libjvm.so";
#else
const char profiler_lib_path[] = "build/lib/libasyncProfiler.dylib";
const char jvm_lib_path[] = "server/libjvm.dylib";
#endif

typedef jint (*CreateJvm)(JavaVM**, void**, void*);

asprof_init_t _asprof_init;
asprof_execute_t _asprof_execute;
asprof_error_str_t _asprof_error_str;

JavaVM* _jvm;
JNIEnv* _env;

void* _jvm_lib;

void outputCallback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

#include <math.h>

double nativeBurnCpu() {
    int i = 0;
    double result = 0;

    while (i < 100000000) {
        i++;
        result += sqrt(i);
        result += pow(i, sqrt(i));
    }
    return result;
}

void loadProfiler() {
    void* lib = dlopen(profiler_lib_path, RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    _asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    _asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    _asprof_error_str = (asprof_error_str_t)dlsym(lib, "asprof_error_str");

    _asprof_init();
}

void startProfiler(char* output_file) {
    asprof_error_t err;

    if (output_file == NULL) {
        err = _asprof_execute("start,event=cpu,interval=1ms,timeout=10s,cstack=vmx", outputCallback);
    } else {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "start,event=cpu,interval=1ms,timeout=10s,cstack=vmx,file=%s", output_file);
        err = _asprof_execute(cmd, outputCallback);
    }

    if (err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str(err));
        exit(1);
    }
}

void stopProfiler(char* output_file) {
    asprof_error_t err;

    if (output_file == NULL) {
        err = _asprof_execute("stop", outputCallback);
    } else {
        char cmd[4096];
        snprintf(cmd, sizeof(cmd), "stop,file=%s", output_file);
        err = _asprof_execute(cmd, outputCallback);
    }

    if (err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str(err));
        exit(1);
    }
}

void dumpProfiler(char* output_file) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "dump,file=%s", output_file);
    asprof_error_t err = _asprof_execute(cmd, outputCallback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", _asprof_error_str(err));
        exit(1);
    }
}

void loadJvmLib() {
    char* java_home = getenv("TEST_JAVA_HOME");
    if (java_home == NULL) {
        fprintf(stderr, "TEST_JAVA_HOME is not set\n");
        exit(1);
    }

    // Check that libjvm is found under the standard path
    char lib_path[PATH_MAX + 280];
    snprintf(lib_path, sizeof(lib_path), "%s/%s/%s", java_home, "lib", jvm_lib_path);
    if ((_jvm_lib = dlopen(lib_path, RTLD_LOCAL | RTLD_NOW)) != NULL) {
        return;
    }

    // JDK 8 has different directory layout. libjvm path will be the following:
    // ${TEST_JAVA_HOME}/lib/${ARCH}/server/libjvm.(so|dylib)
    char java_lib_home[PATH_MAX];
    snprintf(java_lib_home, sizeof(java_lib_home), "%s/lib", java_home);

    DIR* dir = opendir(java_lib_home);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", java_lib_home);
        exit(1);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, "..") == 0 || strcmp(entry->d_name, ".") == 0) {
            continue;
        }

        snprintf(lib_path, sizeof(lib_path), "%s/%s/%s", java_lib_home, entry->d_name, jvm_lib_path);
        if ((_jvm_lib = dlopen(lib_path, RTLD_LOCAL | RTLD_NOW)) != NULL) {
            break;
        }
    }
    closedir(dir);

    if (_jvm_lib == NULL) {
        fprintf(stderr, "Unable to find: libjvm\n");
        exit(1);
    }
}

void startJvm() {
    JavaVMInitArgs vm_args;
    JavaVMOption options[1];

    options[0].optionString = const_cast<char*>("-Djava.class.path=build/test/classes");
    //options[1].optionString = const_cast<char*>("-Xcheck:jni");

    // Configure JVM
    vm_args.version = JNI_VERSION_1_6;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = true;

    CreateJvm create_jvm = (CreateJvm)dlsym(_jvm_lib, "JNI_CreateJavaVM");
    if (create_jvm == NULL) {
        fprintf(stderr, "Unable to find: JNI_CreateJavaVM\n");
        exit(1);
    }

    // Create the JVM
    jint rc = create_jvm(&_jvm, (void**)&_env, &vm_args);
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to create JVM\n");
        exit(1);
    }
}

void executeJvmTask() {
    jclass java_class = _env->FindClass("test/nonjava/JavaClass");
    if (java_class == nullptr) {
        fprintf(stderr, "Can't find JavaClass\n");
        exit(1);
    }

    jmethodID method = _env->GetStaticMethodID(java_class, "cpuHeavyTask", "()D");
    if (method == nullptr) {
        fprintf(stderr, "Can't find cpuHeavyTask\n");
        exit(1);
    }

    for (int i = 0; i < 300; ++i) {
        jdouble result = _env->CallStaticDoubleMethod(java_class, method);
        if (_env->ExceptionCheck()) {
            _env->ExceptionDescribe();
            fprintf(stderr, "Exception in cpuHeavyTask\n");
            exit(1);
        }
        fprintf(stdout, "Result: %.02f\n", result);
    }
    _env->DeleteLocalRef(java_class);
}

void stopJvm() {
    jint rc = _jvm->DestroyJavaVM();
    if (rc != JNI_OK) {
        fprintf(stderr, "Failed to destroy JVM\n");
        exit(1);
    }
}

void validateArgsCount(int argc, int expected) {
    if (argc < expected) {
        fprintf(stderr, "Test requires %d arguments\n", expected - 1);
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
The profiler should be able to profile the JVM task.

Explanation:
The JVM is loaded and started before the profiling session,
so profiler correctly detects the JVM when the profiling session starts.
*/
void testFlow1(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startJvm();

    startProfiler(NULL);

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
The profiler will not be able to sample JVM stacks correctly.

Explanation:
The JVM is not yet created when the profiling session starts,
so the profiler does not attach to the JVM.
*/
void testFlow2(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startProfiler(NULL);

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
The profiler will not be able to sample JVM stacks correctly during the first session,
but it will do during the second session.

Explanation:
It is a combination of the first two flows. The JVM is not yet started
before the first session, but it is started before the second session.
*/
void testFlow3(int argc, char** argv) {
    validateArgsCount(argc, 4);

    loadProfiler();

    loadJvmLib();

    startProfiler(NULL);

    startJvm();

    nativeBurnCpu();
    executeJvmTask();

    stopProfiler(argv[2]);

    startProfiler(NULL);

    nativeBurnCpu();
    executeJvmTask();

    stopProfiler(argv[3]);

    stopJvm();
}

void* jvmThreadWrapper(void* arg) {
    volatile bool* run_jvm = (volatile bool*)arg;

    loadJvmLib();

    startJvm();

    while (*run_jvm) executeJvmTask();

    stopJvm();

    return NULL;
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM on a different thread
4. Start the profiler
5. Execute the JVM task
6. Stop the profiler
7. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task.

Explanation:
The same as flow 1, except that profiler will have to attach the caller thread to the JVM.
*/
void testFlow4(int argc, char** argv) {
    volatile bool run_jvm = true;

    struct timespec wait_time = {(time_t)(2), 0L};

    loadProfiler();

    pthread_t thread;
    pthread_create(&thread, NULL, jvmThreadWrapper, (void*)&run_jvm);

    nanosleep(&wait_time, NULL);

    startProfiler(NULL);

    nanosleep(&wait_time, NULL);

    stopProfiler(argv[2]);

    run_jvm = false;

    pthread_join(thread, NULL);
}


void* dumpProfile(void* arg) {
    char* output_file = (char*)arg;
    dumpProfiler(output_file);
    return NULL;
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM
4. Start the profiler
5. Execute the JVM task
6. Stop the profiler
7. Dump the profiler on a different thread
8. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task.

Explanation:
The JVM is loaded and started before the profiling session,
so profiler correctly dump profiling details related to the JVM process.
*/
void testFlow5(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startJvm();

    startProfiler(NULL);

    executeJvmTask();

    stopProfiler(NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, dumpProfile, argv[2]);
    pthread_join(thread, NULL);

    stopJvm();
}

void* startProfilerDifferentThread(void* arg) {
    startProfiler((char*)arg);
    return NULL;
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM
4. Start the profiler
5. Stop the profiler
6. Start the profiler on a different thread
7. Execute the JVM task
7. Stop the profiler on the original thread
8. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task.

Explanation:
The JVM is loaded and started before the profiling session,
so profiler correctly dump profiling details related to the JVM process.
*/
void testFlow6(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startJvm();

    startProfiler(NULL);

    stopProfiler(NULL);

    pthread_t thread;
    pthread_create(&thread, NULL, startProfilerDifferentThread, NULL);
    pthread_join(thread, NULL);

    executeJvmTask();

    stopProfiler(argv[2]);

    stopJvm();
}

void* stopProfilerDifferentThread(void* arg) {
    stopProfiler((char*)arg);
    return NULL;
}

/*
Here is the flow of the test:
1. Load the profiler
2. Load the JVM library
3. Start the JVM
4. Start the profiler
5. Stop the profiler
6. Start the profiler on a different thread
7. Execute the JVM task
8. Stop the profiler on a different thread
9. Stop the JVM

Expected output:
The profiler should be able to profile the JVM task.

Explanation:
The JVM is loaded and started before the profiling session,
so profiler correctly dump profiling details related to the JVM process.
*/
void testFlow7(int argc, char** argv) {
    loadProfiler();

    loadJvmLib();

    startJvm();

    startProfiler(NULL);

    stopProfiler(NULL);

    pthread_t thread1;
    pthread_create(&thread1, NULL, startProfilerDifferentThread, argv[2]);
    pthread_join(thread1, NULL);

    executeJvmTask();

    pthread_t thread2;
    pthread_create(&thread2, NULL, stopProfilerDifferentThread, NULL);
    pthread_join(thread2, NULL);

    stopJvm();
}

int main(int argc, char** argv) {
    validateArgsCount(argc, 3);

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
        case '5':
            testFlow5(argc, argv);
            break;
        case '6':
            testFlow6(argc, argv);
            break;
        case '7':
            testFlow7(argc, argv);
            break;
        default:
            fprintf(stderr, "Unknown flow: %s\n", flow);
            exit(1);
    }

    return 0;
}
