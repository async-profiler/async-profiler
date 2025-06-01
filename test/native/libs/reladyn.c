/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <pthread.h>
#include <stdio.h>

// Force pthread_setspecific into .rela.dyn with R_X86_64_GLOB_DAT.
int (*indirect_pthread_setspecific)(pthread_key_t, const void*);

// Force pthread_exit into .rela.dyn with R_X86_64_64.
void (*static_pthread_exit)(void*) = pthread_exit;

void* thread_function(void* arg) {
    printf("Thread running\n");
    return NULL;
}

// Not indended to be executed.
int reladyn() {
    pthread_t thread;
    pthread_key_t key;

    pthread_key_create(&key, NULL);

    // Direct call, forces into .rela.plt.
    pthread_create(&thread, NULL, thread_function, NULL);

    // Assign to a function pointer at runtime, forces into .rela.dyn as R_X86_64_GLOB_DAT.
    indirect_pthread_setspecific = pthread_setspecific;
    indirect_pthread_setspecific(key, "Thread-specific value");

    // Use pthread_exit via the static pointer, forces into .rela.dyn as R_X86_64_64.
    static_pthread_exit(NULL);

    return 0;
}
