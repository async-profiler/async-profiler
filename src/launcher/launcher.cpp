/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../incbin.h"


#define APP_BINARY "jfrconv"

static const char VERSION_STRING[] =
    "JFR converter " PROFILER_VERSION " built on " __DATE__ "\n";

INCLUDE_HELPER_CLASS(EMBEDDED_CLASS_LOADER, CLASS_BYTES, "one/profiler/EmbeddedClassLoader")
INCBIN(CONVERTER_JAR, "build/jar/jfr-converter.jar")


#if defined(__APPLE__)
// There is no arch subdirectory in macOS JDK bundles
#define ARCH ""
#elif defined(__x86_64__)
#define ARCH "amd64"
#elif defined(__i386__)
#define ARCH "i386"
#elif defined(__arm__) || defined(__thumb__)
#define ARCH "arm"
#elif defined(__aarch64__)
#define ARCH "aarch64"
#elif defined(__PPC64__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define ARCH "ppc64le"
#elif defined(__riscv) && (__riscv_xlen == 64)
#define ARCH "riscv64"
#elif defined(__loongarch_lp64)
#define ARCH "loongarch64"
#endif

#if defined(__APPLE__)
#define COMMON_JVM_DIR "/Library/Java/JavaVirtualMachines/"
#define CONTENTS_HOME  "/Contents/Home"
#define LIBJVM         "libjvm.dylib"
#else
#define COMMON_JVM_DIR "/usr/lib/jvm/"
#define CONTENTS_HOME  ""
#define LIBJVM         "libjvm.so"
#endif


typedef jint (*JNI_CreateJavaVM_t)(JavaVM **p_vm, void **p_env, void *vm_args);

static void* load_libjvm(const char* java_home, const char* subdir) {
    char buf[PATH_MAX];
    if (snprintf(buf, sizeof(buf), "%s/%s/" LIBJVM, java_home, subdir) >= sizeof(buf)) {
        return NULL;
    }

    struct stat statbuf;
    if (stat(buf, &statbuf) != 0) {
        return NULL;
    }

    return dlopen(buf, RTLD_NOW);
}

static void* find_libjvm_at(const char* path, const char* path1 = "", const char* path2 = "") {
    char buf[PATH_MAX];
    if (snprintf(buf, sizeof(buf), "%s%s%s", path, path1, path2) >= sizeof(buf)) {
        return NULL;
    }

    char* java_home = realpath(buf, NULL);
    if (java_home == NULL) {
        return NULL;
    }

    char* p = strrchr(java_home, '/');
    if (p != NULL) {
        *p = 0;  // strip /java
    }
    if ((p = strrchr(java_home, '/')) != NULL) {
        *p = 0;  // strip /bin
    }

    void* libjvm;
    if ((libjvm = load_libjvm(java_home, "lib/server")) == NULL &&
        (libjvm = load_libjvm(java_home, "lib/client")) == NULL &&
        (libjvm = load_libjvm(java_home, "lib/" ARCH "/server")) == NULL &&
        (libjvm = load_libjvm(java_home, "lib/" ARCH "/client")) == NULL &&
        (libjvm = load_libjvm(java_home, "jre/lib/" ARCH "/server")) == NULL &&
        (libjvm = load_libjvm(java_home, "jre/lib/" ARCH "/client")) == NULL) {
        // No libjvm.so found at this path
    }

    free(java_home);
    return libjvm;
}

static void* find_libjvm() {
    void* libjvm;
    char* java_home = getenv("JAVA_HOME");
    if (java_home != NULL && (libjvm = find_libjvm_at(java_home, "/bin/java")) != NULL) {
        return libjvm;
    }

    char* path = getenv("PATH");
    char* path_copy;
    if (path != NULL && (path_copy = strdup(path)) != NULL) {
        for (char* java_bin = strtok(path_copy, ":"); java_bin != NULL; java_bin = strtok(NULL, ":")) {
            if ((libjvm = find_libjvm_at(java_bin, "/java")) != NULL) {
                free(path_copy);
                return libjvm;
            }
        }
        free(path_copy);
    }

    if ((libjvm = find_libjvm_at("/etc/alternatives/java")) != NULL) {
        return libjvm;
    }

    DIR* dir = opendir(COMMON_JVM_DIR);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.' && entry->d_type == DT_DIR) {
                if ((libjvm = find_libjvm_at(COMMON_JVM_DIR, entry->d_name, CONTENTS_HOME "/bin/java")) != NULL) {
                    closedir(dir);
                    return libjvm;
                }
            }
        }
        closedir(dir);
    }

    return NULL;
}

static int print_exception(JavaVM* vm, JNIEnv* env) {
    env->ExceptionDescribe();
    vm->DestroyJavaVM();
    return 1;
}

static int run_jvm(void* libjvm, int argc, char** argv) {
    JNI_CreateJavaVM_t JNI_CreateJavaVM = (JNI_CreateJavaVM_t)dlsym(libjvm, "JNI_CreateJavaVM");
    if (JNI_CreateJavaVM == NULL) {
        return 1;
    }

    JavaVMOption options[argc + 2];
    int o_count = 0;
    options[o_count++].optionString = (char*)"-Dsun.java.command=" APP_BINARY;
    options[o_count++].optionString = (char*)"-Xss2M";

    for (; argc > 0; argc--, argv++) {
        if ((strncmp(*argv, "-D", 2) == 0 || strncmp(*argv, "-X", 2) == 0) && (*argv)[2] ||
                strncmp(*argv, "-agent", 6) == 0) {
            options[o_count++].optionString = *argv;
        } else if (strncmp(*argv, "-J", 2) == 0) {
            options[o_count++].optionString = *argv + 2;
        } else {
            break;
        }
    }

    JavaVM* vm;
    JNIEnv* env;
    JavaVMInitArgs args = {
        .version = JNI_VERSION_1_6,
        .nOptions = o_count,
        .options = options,
        .ignoreUnrecognized = JNI_TRUE
    };

    int res = JNI_CreateJavaVM(&vm, (void**)&env, &args);
    if (res != 0) {
        return res;
    }

    jclass loader = env->DefineClass(EMBEDDED_CLASS_LOADER, NULL, (const jbyte*)CLASS_BYTES, INCBIN_SIZEOF(CLASS_BYTES));
    if (loader == NULL) {
        return print_exception(vm, env);
    }

    jmethodID load_main = env->GetStaticMethodID(loader, "loadMainClass", "(Ljava/nio/ByteBuffer;)Ljava/lang/Class;");
    if (load_main == NULL) {
        return print_exception(vm, env);
    }

    jobject jar = env->NewDirectByteBuffer((void*)CONVERTER_JAR, INCBIN_SIZEOF(CONVERTER_JAR));
    if (jar == NULL) {
        return print_exception(vm, env);
    }

    jclass main_class = (jclass)env->CallStaticObjectMethod(loader, load_main, jar);
    if (main_class == NULL) {
        return print_exception(vm, env);
    }

    jmethodID main_method = env->GetStaticMethodID(main_class, "main", "([Ljava/lang/String;)V");
    if (main_method == NULL) {
        return print_exception(vm, env);
    }

    jobjectArray main_args = env->NewObjectArray(argc, env->FindClass("java/lang/String"), NULL);
    if (main_args != NULL) {
        for (int i = 0; i < argc; i++) {
            env->SetObjectArrayElement(main_args, i, env->NewStringUTF(argv[i]));
        }
    }
    if (env->ExceptionCheck()) {
        return print_exception(vm, env);
    }

    env->CallStaticVoidMethod(main_class, main_method, main_args);
    if (env->ExceptionCheck()) {
        return print_exception(vm, env);
    }

    vm->DestroyJavaVM();
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf(VERSION_STRING);
        return 0;
    }

    void* libjvm = find_libjvm();
    if (libjvm == NULL) {
        fprintf(stderr, "No JDK found. Set JAVA_HOME or ensure java executable is on the PATH.\n");
        return 1;
    }

    return run_jvm(libjvm, argc - 1, argv + 1);
}
