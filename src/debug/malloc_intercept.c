#include <dlfcn.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "debug.h"

static int sighandler_tid = -1;

extern void *__libc_malloc(size_t size);

void *malloc(size_t size) {
    void *result = NULL;
    int tid = sighandler_tid != -1 ? syscall(__NR_gettid) : -2;
    if (tid == sighandler_tid) {
        fprintf(stderr, "!!!! MALLOC in signal handler !!!\n");
        raise(SIGSEGV);
    } else {
        result = __libc_malloc(size);
    }

    return result;
}

void set_sighandler_tid(int tid) {
    sighandler_tid = tid;
}