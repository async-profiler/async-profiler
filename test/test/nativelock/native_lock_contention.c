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
    if (argc < 2) {
        printf("Usage: %s <mutex|rwlock>\n", argv[0]);
        return 1;
    }

    usleep(50000);

    if (strcmp(argv[1], "mutex") == 0) {
        printf("Testing mutex contention...\n");
        
        const int num_threads = 4;
        pthread_t threads[num_threads];
        
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, mutex_contention_thread, NULL);
        }
        
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
    } else if (strcmp(argv[1], "rwlock") == 0) {
        printf("Testing rwlock contention...\n");
        
        const int num_readers = 4;
        const int num_writers = 2;
        pthread_t threads[num_readers + num_writers];
        
        for (int i = 0; i < num_readers; i++) {
            pthread_create(&threads[i], NULL, rwlock_reader_thread, NULL);
        }
        
        for (int i = 0; i < num_writers; i++) {
            pthread_create(&threads[num_readers + i], NULL, rwlock_writer_thread, NULL);
        }
        
        for (int i = 0; i < num_readers + num_writers; i++) {
            pthread_join(threads[i], NULL);
        }
        
    } else {
        printf("Invalid test type: %s\n", argv[1]);
        return 1;
    }

    printf("Test completed successfully.\n");
    return 0;
}
