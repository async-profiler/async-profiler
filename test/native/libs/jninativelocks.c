/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jni.h>
#include <pthread.h>
#include <unistd.h>

static pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;

void* contention_thread(void* arg) {
    for (int i = 0; i < 200; i++) {
        pthread_mutex_lock(&global_mutex);
        usleep(5000);
        pthread_mutex_unlock(&global_mutex);
        usleep(1000);
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_createMutexContention(JNIEnv *env, jclass cls) {
    const int num_threads = 4;
    pthread_t threads[num_threads];
    
    // Small delay to ensure profiler hooks are ready
    usleep(50000);
    
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, contention_thread, NULL);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

static pthread_rwlock_t global_rwlock = PTHREAD_RWLOCK_INITIALIZER;

void* rdlock_contention_thread(void* arg) {
    for (int i = 0; i < 100; i++) {
        pthread_rwlock_rdlock(&global_rwlock);
        usleep(3000);
        pthread_rwlock_unlock(&global_rwlock);
        usleep(1000);
    }
    return NULL;
}

void* wrlock_contention_thread(void* arg) {
    for (int i = 0; i < 50; i++) {
        pthread_rwlock_wrlock(&global_rwlock);
        usleep(5000);
        pthread_rwlock_unlock(&global_rwlock);
        usleep(2000);
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_createRdLockContention(JNIEnv *env, jclass cls) {
    const int num_readers = 6;
    const int num_writers = 2;
    pthread_t threads[num_readers + num_writers];
    
    for (int i = 0; i < num_readers; i++) {
        pthread_create(&threads[i], NULL, rdlock_contention_thread, NULL);
    }
    
    for (int i = 0; i < num_writers; i++) {
        pthread_create(&threads[num_readers + i], NULL, wrlock_contention_thread, NULL);
    }

    for (int i = 0; i < num_readers + num_writers; i++) {
        pthread_join(threads[i], NULL);
    }
}

JNIEXPORT void JNICALL Java_test_nativelock_NativeLock_createWrLockContention(JNIEnv *env, jclass cls) {
    const int num_writers = 4;
    pthread_t threads[num_writers];
    
    for (int i = 0; i < num_writers; i++) {
        pthread_create(&threads[i], NULL, wrlock_contention_thread, NULL);
    }
    
    for (int i = 0; i < num_writers; i++) {
        pthread_join(threads[i], NULL);
    }
}
