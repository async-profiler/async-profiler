/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <jni.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

static size_t page;
static void* retained;
#define CHECK(expr) do { if (!(expr)) abort(); } while (0)

// A stable native stack frame for the converter filter, including unattached pthreads.
__attribute__((noinline)) void* native_fixture_map(void* address, size_t size, int extra) {
    void* result = mmap(address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | extra, -1, 0);
    CHECK(result != MAP_FAILED);
    return result;
}

static void* worker(void* ignored) {
    for (int i = 0; i < 100; i++) {
        char* p = native_fixture_map(NULL, page * 3, 0);
        CHECK(munmap(p + page, page) == 0);
        CHECK(munmap(p, page) == 0);
        CHECK(munmap(p + page * 2, page) == 0);
    }
    return NULL;
}

static void* release_other_thread(void* p) {
    CHECK(munmap(p, page * 3) == 0);
    return NULL;
}

#ifdef __linux__
__attribute__((noinline)) static void native_fixture_map64(void) {
    void* p = mmap64(NULL, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(p != MAP_FAILED);
    CHECK(munmap(p, page) == 0);
}
#endif

JNIEXPORT jlong JNICALL Java_probe_NativeMappings_exercise(JNIEnv* env, jclass cls) {
    page = sysconf(_SC_PAGESIZE);
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) CHECK(pthread_create(&threads[i], NULL, worker, NULL) == 0);
    for (int i = 0; i < 4; i++) CHECK(pthread_join(threads[i], NULL) == 0);
    void* other = native_fixture_map(NULL, page * 3, 0);
    CHECK(pthread_create(&threads[0], NULL, release_other_thread, other) == 0);
    CHECK(pthread_join(threads[0], NULL) == 0);
    char* replaced = native_fixture_map(NULL, page * 3, 0);
    CHECK(native_fixture_map(replaced + page, page, MAP_FIXED) == replaced + page);
    CHECK(munmap(replaced, page * 3) == 0);
#ifdef __linux__
    native_fixture_map64();
#endif
    errno = 0;
    CHECK(mmap(NULL, 0, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == MAP_FAILED);
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(munmap((void*)1, page) == -1);
    CHECK(errno == EINVAL);
    retained = native_fixture_map(NULL, page, 0);
    return page;
}

JNIEXPORT void JNICALL Java_probe_NativeMappings_cleanup(JNIEnv* env, jclass cls) {
    CHECK(munmap(retained, page) == 0);
}
