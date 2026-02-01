/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t global_rwlock = PTHREAD_RWLOCK_INITIALIZER;

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_mutexContentionThread(JNIEnv *env, jclass cls) {
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&global_mutex);
        usleep(3000);
        pthread_mutex_unlock(&global_mutex);
        usleep(1000);
    }
}

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_rdlockContentionThread(JNIEnv *env, jclass cls) {
    for (int i = 0; i < 50; i++) {
        pthread_rwlock_rdlock(&global_rwlock);
        usleep(3000);
        pthread_rwlock_unlock(&global_rwlock);
        usleep(1000);
    }
}

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_wrlockContentionThread(JNIEnv *env, jclass cls) {
    for (int i = 0; i < 50; i++) {
        pthread_rwlock_wrlock(&global_rwlock);
        usleep(5000);
        pthread_rwlock_unlock(&global_rwlock);
        usleep(2000);
    }
}
