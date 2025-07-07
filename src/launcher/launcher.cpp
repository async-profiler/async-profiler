/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <jni.h>


#ifdef __APPLE__
#  include <mach-o/dyld.h>
#  define COMMON_JVM_DIR "/Library/Java/JavaVirtualMachines/"
#  define CONTENTS_HOME  "/Contents/Home"
#else
#  define COMMON_JVM_DIR "/usr/lib/jvm/"
#  define CONTENTS_HOME  ""
#endif

#define JAVA_EXE "java"


static const char VERSION_STRING[] =
    "JFR converter " PROFILER_VERSION " built on " __DATE__ "\n";


static char exe_path[PATH_MAX];
static char java_path[PATH_MAX];

extern "C" {
    extern char jar_data_start[];
    extern char jar_data_end[];
}

static JavaVM* create_jvm() {
    JavaVMInitArgs vm_args;
    JavaVMOption options[3];
    
    options[0].optionString = const_cast<char*>("-Xss2M");
    options[1].optionString = const_cast<char*>("-Dsun.misc.URLClassPath.disableJarChecking=true");
    options[2].optionString = const_cast<char*>("-Djava.class.path=/tmp");
    
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 3;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;
    
    JavaVM* jvm;
    JNIEnv* env;
    
    if (JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args) != JNI_OK) {
        return NULL;
    }
    
    return jvm;
}

static int run_main_class(JavaVM* jvm, int argc, char** argv) {
    JNIEnv* env;
    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return 1;
    }
    
    // Find Main class
    jclass mainClass = env->FindClass("Main");
    if (!mainClass) {
        fprintf(stderr, "Could not find Main class\n");
        return 1;
    }
    
    // Get main method
    jmethodID mainMethod = env->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    if (!mainMethod) {
        fprintf(stderr, "Could not find main method\n");
        return 1;
    }
    
    // Create String array for arguments
    jobjectArray args = env->NewObjectArray(argc, env->FindClass("java/lang/String"), NULL);
    for (int i = 0; i < argc; i++) {
        jstring arg = env->NewStringUTF(argv[i]);
        env->SetObjectArrayElement(args, i, arg);
    }
    
    // Call main method
    env->CallStaticVoidMethod(mainClass, mainMethod, args);
    
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return 1;
    }
    
    return 0;
}

static bool extract_jar_to_temp() {
    char temp_jar[] = "/tmp/jfr-converter-XXXXXX.jar";
    int fd = mkstemps(temp_jar, 4);
    if (fd == -1) {
        return false;
    }
    
    size_t jar_size = jar_data_end - jar_data_start;
    if (write(fd, jar_data_start, jar_size) != (ssize_t)jar_size) {
        close(fd);
        unlink(temp_jar);
        return false;
    }
    close(fd);
    
    strcpy(exe_path, temp_jar);
    return true;
}



int main(int argc, char** argv) {
    if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf(VERSION_STRING);
        return 0;
    }

    if (!extract_jar_to_temp()) {
        fprintf(stderr, "Failed to extract JAR\n");
        return 1;
    }

    JavaVM* jvm = create_jvm();
    if (!jvm) {
        fprintf(stderr, "Failed to create JVM\n");
        return 1;
    }

    int result = run_main_class(jvm, argc - 1, argv + 1);
    
    jvm->DestroyJavaVM();
    unlink(exe_path); // Clean up temp JAR
    
    return result;
}
