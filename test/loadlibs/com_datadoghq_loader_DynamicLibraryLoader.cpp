#include <jni.h>
#include "com_datadoghq_loader_DynamicLibraryLoader.h"
#include <iostream>
#include <dlfcn.h>

JNIEXPORT void JNICALL Java_com_datadoghq_loader_DynamicLibraryLoader_loadLibrary(JNIEnv* env, jobject unused, jstring library, jstring name) {
    int (*function)(int i);
    void* handle = dlopen((char *) library, RTLD_LAZY);
    if (NULL == handle) {
        std::cout << "could not load " << library << std::endl;
    } else {
        function = (int(*)(int)) dlsym(handle, name);
        int next = (*function)(1);
        std::cout << "loaded " << library << ": " << next << std::endl;
    }
}

