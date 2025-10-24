/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static pthread_mutex_t test_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_rwlock_t test_rwlock = PTHREAD_RWLOCK_INITIALIZER;

void* mutex_contention_thread(void* arg) {
    for (int i = 0; i < 50; i++) {
        pthread_mutex_lock(&test_mutex);
        usleep(5000);
        pthread_mutex_unlock(&test_mutex);
        usleep(1000);
    }
    return NULL;
}

void* rwlock_reader_thread(void* arg) {
    for (int i = 0; i < 30; i++) {
        pthread_rwlock_rdlock(&test_rwlock);
        usleep(3000);
        pthread_rwlock_unlock(&test_rwlock);
        usleep(1000);
    }
    return NULL;
}

void* rwlock_writer_thread(void* arg) {
    for (int i = 0; i < 15; i++) {
        pthread_rwlock_wrlock(&test_rwlock);
        usleep(5000);
        pthread_rwlock_unlock(&test_rwlock);
        usleep(2000);
    }
    return NULL;
}

int main(int argc, char* argv[]) {

    printf("Testing all lock types...\n");
    printf("Testing mutex contention...\n");
    const int mutex_threads = 4;
    pthread_t mutex_thread_array[mutex_threads];
    
    for (int i = 0; i < mutex_threads; i++) {
        pthread_create(&mutex_thread_array[i], NULL, mutex_contention_thread, NULL);
    }
    
    for (int i = 0; i < mutex_threads; i++) {
        pthread_join(mutex_thread_array[i], NULL);
    }
    
    printf("Testing rwlock contention...\n");
    const int num_readers = 4;
    const int num_writers = 2;
    pthread_t rwlock_threads[num_readers + num_writers];
    
    for (int i = 0; i < num_readers; i++) {
        pthread_create(&rwlock_threads[i], NULL, rwlock_reader_thread, NULL);
    }
    
    for (int i = 0; i < num_writers; i++) {
        pthread_create(&rwlock_threads[num_readers + i], NULL, rwlock_writer_thread, NULL);
    }
    
    for (int i = 0; i < num_readers + num_writers; i++) {
        pthread_join(rwlock_threads[i], NULL);
    }

    fprintf(stderr, "Test completed successfully.\n");
    return 0;
}
