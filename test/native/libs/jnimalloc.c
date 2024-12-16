/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <stdint.h>
#include <stdlib.h>

JNIEXPORT jlong JNICALL Java_test_nativemem_Native_malloc(JNIEnv* env, jclass clazz, jlong size) {
    void* ptr = malloc((size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_Native_calloc(JNIEnv* env, jclass clazz, jlong num, jlong size) {
    void* ptr = calloc(num, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_Native_realloc(JNIEnv* env, jclass clazz, jlong addr, jlong size) {
    void* ptr = realloc((void*)(intptr_t)addr, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT void JNICALL Java_test_nativemem_Native_free(JNIEnv* env, jclass clazz, jlong addr) {
    free((void*)(intptr_t)addr);
}
