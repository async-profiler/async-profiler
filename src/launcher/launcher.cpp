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




extern "C" {
    extern char jar_data_start[];
    extern char jar_data_end[];
}

static JavaVM* create_jvm() {
    JavaVMInitArgs vm_args;
    JavaVMOption options[2];
    
    static char option1[] = "-Xss2M";
    static char option2[] = "-Dsun.misc.URLClassPath.disableJarChecking=true";
    options[0].optionString = option1;
    options[1].optionString = option2;
    
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 2;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_FALSE;
    
    JavaVM* jvm;
    JNIEnv* env;
    
    if (JNI_CreateJavaVM(&jvm, reinterpret_cast<void**>(&env), &vm_args) != JNI_OK) {
        return nullptr;
    }
    
    return jvm;
}

static int run_main_class(JavaVM* jvm, int argc, char** argv) {
    JNIEnv* env;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return 1;
    }
    
    // Create ConverterClassLoader with embedded JAR data
    jclass loader_class = env->FindClass("ConverterClassLoader");
    if (loader_class == nullptr) {
        fprintf(stderr, "Could not find ConverterClassLoader class\n");
        return 1;
    }
    
    jmethodID loader_constructor = env->GetMethodID(loader_class, "<init>", "([B)V");
    if (loader_constructor == nullptr) {
        fprintf(stderr, "Could not find ConverterClassLoader constructor\n");
        return 1;
    }
    
    // Create byte array from embedded JAR data
    size_t jar_size = jar_data_end - jar_data_start;
    jbyteArray jar_bytes = env->NewByteArray(static_cast<jsize>(jar_size));
    env->SetByteArrayRegion(jar_bytes, 0, static_cast<jsize>(jar_size), reinterpret_cast<const jbyte*>(jar_data_start));
    
    // Create ConverterClassLoader instance
    jobject class_loader = env->NewObject(loader_class, loader_constructor, jar_bytes);
    if (class_loader == nullptr) {
        fprintf(stderr, "Could not create ConverterClassLoader instance\n");
        return 1;
    }
    
    // Load Main class using custom class loader
    jmethodID load_class_method = env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (load_class_method == nullptr) {
        fprintf(stderr, "Could not find loadClass method\n");
        return 1;
    }
    
    jstring main_class_name = env->NewStringUTF("Main");
    auto* main_class = static_cast<jclass>(env->CallObjectMethod(class_loader, load_class_method, main_class_name));
    if (main_class == nullptr) {
        fprintf(stderr, "Could not load Main class\n");
        return 1;
    }
    
    // Get main method
    jmethodID main_method = env->GetStaticMethodID(main_class, "main", "([Ljava/lang/String;)V");
    if (main_method == nullptr) {
        fprintf(stderr, "Could not find main method\n");
        return 1;
    }
    
    // Create String array for arguments
    jobjectArray args = env->NewObjectArray(argc, env->FindClass("java/lang/String"), nullptr);
    for (int i = 0; i < argc; i++) {
        jstring arg = env->NewStringUTF(argv[i]);
        env->SetObjectArrayElement(args, i, arg);
    }
    
    // Call main method
    env->CallStaticVoidMethod(main_class, main_method, args);
    
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        return 1;
    }
    
    return 0;
}





int main(int argc, char** argv) {
    if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf(VERSION_STRING);
        return 0;
    }

    JavaVM* jvm = create_jvm();
    if (jvm == nullptr) {
        fprintf(stderr, "Failed to create JVM\n");
        return 1;
    }

    int result = run_main_class(jvm, argc - 1, argv + 1);
    
    jvm->DestroyJavaVM();
    
    return result;
}
