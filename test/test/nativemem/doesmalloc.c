/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <stdint.h>
#include <stdlib.h>

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsMallocCalloc_nativeMalloc(JNIEnv* env, jclass clazz, jlong size) {
    void* ptr = malloc((size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsMallocCalloc_nativeCalloc(JNIEnv* env, jclass clazz, jlong num, jlong size) {
    void* ptr = calloc(num, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsRealloc_nativeMalloc(JNIEnv* env, jclass clazz, jlong size) {
    void* ptr = malloc((size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsRealloc_nativeRealloc(JNIEnv* env, jclass clazz, jlong addr, jlong size) {
    void* ptr = realloc((void*)addr, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsAllNoLeak_nativeMalloc(JNIEnv* env, jclass clazz, jlong size) {
    void* ptr = malloc((size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsAllNoLeak_nativeCalloc(JNIEnv* env, jclass clazz, jlong num, jlong size) {
    void* ptr = calloc(num, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT jlong JNICALL Java_test_nativemem_CallsAllNoLeak_nativeRealloc(JNIEnv* env, jclass clazz, jlong addr, jlong size) {
    void* ptr = realloc((void*)addr, (size_t)size);
    return (jlong)(intptr_t)ptr;
}

JNIEXPORT void JNICALL Java_test_nativemem_CallsAllNoLeak_nativeFree(JNIEnv* env, jclass clazz, jlong addr) {
    free((void*)addr);
}
