/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/mman.h>
#include <string.h>
#include "incbin.h"
#include "log.h"
#include "mmapTracer.h"
#include "profiler.h"
#include "tsc.h"
#include "vmEntry.h"
#include "mmapRewriter.h"
#include "mutex.h"
#include "symbols.h"

INCLUDE_HELPER_CLASS(MMAP_BRIDGE_NAME, MMAP_BRIDGE_CLASS, "one/profiler/MmapBridge")
std::atomic<bool> MmapTracer::running(false);
std::atomic<bool> MmapTracer::transforming(false);
std::atomic<unsigned long long> MmapTracer::lost(0);
static void* (*mmap_original)(void*, size_t, int, int, int, off_t);
#ifdef __linux__
static void* (*mmap64_original)(void*, size_t, int, int, int, off64_t);
#endif
static int (*munmap_original)(void*, size_t);
static pthread_mutex_t mmap_mutex = PTHREAD_MUTEX_INITIALIZER;
static thread_local bool mmap_nested = false;
static bool mmap_initialized = false;
static Mutex mmap_patch_lock;
static int mmap_patched_libs = 0;
static std::atomic<bool> mmap_transform_failed(false);

// Serialize the covered calls, including the syscall, so post-call events cannot
// invert address reuse between participating threads. Not a global VM monitor.
static void record_mapping(void* address, size_t size, bool unmap) {
    if (size > (size_t)-1 - OS::page_mask) { MmapTracer::lost++; return; }
    MmapEvent event;
    event._start_time = TSC::ticks();
    event._address = (uintptr_t)address;
    event._size = (size + OS::page_mask) & ~OS::page_mask;
    event._unmap = unmap;
    // Native callers need the same stack-walking path as malloc. Frees need no stack.
    bool recorded = unmap ? Profiler::instance()->recordEventOnly(MMAP_SAMPLE, &event)
                          : Profiler::instance()->recordSample(NULL, event._size, MMAP_SAMPLE, &event) != 0;
    if (!recorded) {
        MmapTracer::lost++;
    }
}
template<typename Offset>
static void* mapped_call(void* (*original)(void*, size_t, int, int, int, Offset),
                         void* addr, size_t size, int prot, int flags, int fd, Offset offset) {
    if (mmap_nested || !MmapTracer::running) return original(addr, size, prot, flags, fd, offset);
    mmap_nested = true;
    pthread_mutex_lock(&mmap_mutex);
    void* result = original(addr, size, prot, flags, fd, offset);
    int saved_errno = errno;
    if (MmapTracer::running && result != MAP_FAILED) {
#ifdef MAP_HUGETLB
        if (flags & MAP_HUGETLB) MmapTracer::lost++; // explicit huge pages require different rounding
        else
#endif
        record_mapping(result, size, false);
    }
    pthread_mutex_unlock(&mmap_mutex);
    mmap_nested = false;
    errno = saved_errno;
    return result;
}
extern "C" void* mmap_hook(void* addr, size_t size, int prot, int flags, int fd, off_t offset) {
    return mapped_call(mmap_original, addr, size, prot, flags, fd, offset);
}
#ifdef __linux__
extern "C" void* mmap64_hook(void* addr, size_t size, int prot, int flags, int fd, off64_t offset) {
    return mapped_call(mmap64_original, addr, size, prot, flags, fd, offset);
}
#endif
extern "C" int munmap_hook(void* addr, size_t size) {
    if (mmap_nested || !MmapTracer::running) return munmap_original(addr, size);
    mmap_nested = true;
    pthread_mutex_lock(&mmap_mutex);
    int result = munmap_original(addr, size);
    int saved_errno = errno;
    if (MmapTracer::running && result == 0) record_mapping(addr, size, true);
    pthread_mutex_unlock(&mmap_mutex);
    mmap_nested = false;
    errno = saved_errno;
    return result;
}
static jlong JNICALL mmap_address(JNIEnv* jni, jclass unused, jobject function) {
    jclass cls = jni->GetObjectClass(function);
    jfieldID field = jni->GetFieldID(cls, "peer", "J");
    jni->DeleteLocalRef(cls);
    if (field == NULL) return 0; // pending exception, never invoke an unknown address
    jlong address = jni->GetLongField(function, field);
    if (!MmapTracer::running) return address;
    if ((uintptr_t)address == (uintptr_t)mmap_original) return (jlong)(uintptr_t)mmap_hook;
#ifdef __linux__
    if ((uintptr_t)address == (uintptr_t)mmap64_original) return (jlong)(uintptr_t)mmap64_hook;
#endif
    if ((uintptr_t)address == (uintptr_t)munmap_original) return (jlong)(uintptr_t)munmap_hook;
    return address;
}
static bool retransform_jna() {
    jvmtiEnv* jvmti = VM::jvmti();
    jint count;
    jclass* classes;
    if (jvmti->GetLoadedClasses(&count, &classes) != JVMTI_ERROR_NONE) return false;
    bool ok = true;
    int matched = 0;
    for (int i = 0; i < count; i++) {
        char* name = NULL;
        if (jvmti->GetClassSignature(classes[i], &name, NULL) == JVMTI_ERROR_NONE) {
            if (strcmp(name, "Lcom/sun/jna/Function;") == 0) {
                matched++;
                if (jvmti->RetransformClasses(1, classes + i) != JVMTI_ERROR_NONE) ok = false;
            }
            jvmti->Deallocate((unsigned char*)name);
        }
        VM::jni()->DeleteLocalRef(classes[i]);
    }
    jvmti->Deallocate((unsigned char*)classes);
    Log::info("mmap: JNA Function classes found: %d", matched);
    return ok;
}
void MmapTracer::installHooks() {
    if (!running) return;
    MutexLocker lock(mmap_patch_lock);
    if (!running) return;
    // Loader/patcher bookkeeping must not enter the recording path recursively.
    bool nested = mmap_nested;
    mmap_nested = true;
    CodeCacheArray* libs = Profiler::instance()->nativeLibs();
    int count = libs->count();
    while (mmap_patched_libs < count) {
        CodeCache* lib = (*libs)[mmap_patched_libs++];
        // Our own storage uses mappings too. Never patch the profiler's imports.
        if (lib->contains((void*)mmap_hook)) continue;
        UnloadProtection protection(lib);
        if (!protection.isValid()) continue;
        const ImportId ids[] = {im_mmap, im_mmap64, im_munmap};
        const char* names[] = {"mmap", "mmap64", "munmap"};
        void* hooks[] = {(void*)mmap_hook,
#ifdef __linux__
                        (void*)mmap64_hook,
#else
                        NULL,
#endif
                        (void*)munmap_hook};
        for (unsigned i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
            if (hooks[i] == NULL) continue;
            void** slot = lib->findImport(ids[i]);
            if (slot == NULL) continue;
            lib->patchImport(ids[i], hooks[i]);
            if (*slot != hooks[i]) {
                lost++;
                Log::warn("mmap: could not patch %s in %s", names[i], lib->name());
            } else {
                Log::debug("mmap: patched %s in %s", names[i], lib->name());
            }
        }
    }
    mmap_nested = nested;
}

Error MmapTracer::start(Arguments& args) {
    if (!VM::loaded()) return Error("mmap requires a JVM");
    lost = 0;
    mmap_transform_failed = false;
    if (!mmap_initialized) {
        mmap_original = (decltype(mmap_original))dlsym(RTLD_DEFAULT, "mmap");
        munmap_original = (decltype(munmap_original))dlsym(RTLD_DEFAULT, "munmap");
#ifdef __linux__
        mmap64_original = (decltype(mmap64_original))dlsym(RTLD_DEFAULT, "mmap64");
        if (!mmap64_original) return Error("Cannot resolve mmap64");
#endif
        if (!mmap_original || !munmap_original) return Error("Cannot resolve mmap/munmap");
        CodeCache* lib = Profiler::instance()->findLibraryByAddress((void*)mmap_hook);
        if (lib != NULL) lib->mark([](const char* s) -> bool {
            return strcmp(s, "mmap_hook") == 0 || strcmp(s, "mmap64_hook") == 0 || strcmp(s, "munmap_hook") == 0;
        }, MARK_ASYNC_PROFILER);
        JNIEnv* jni = VM::jni();
        jclass cls = jni->DefineClass(MMAP_BRIDGE_NAME, NULL, (const jbyte*)MMAP_BRIDGE_CLASS, INCBIN_SIZEOF(MMAP_BRIDGE_CLASS));
        JNINativeMethod method = {(char*)"address", (char*)"(Ljava/lang/Object;)J", (void*)mmap_address};
        if (!cls || jni->RegisterNatives(cls, &method, 1) != 0) {
            jni->ExceptionClear();
            return Error("Cannot initialize mmap bootstrap helper");
        }
        jni->DeleteLocalRef(cls);
        mmap_initialized = true;
    }
    transforming = true;
    if (VM::jvmti()->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, NULL) != JVMTI_ERROR_NONE ||
        !retransform_jna() || mmap_transform_failed) return Error("Cannot instrument JNA Function for mmap");
    {
        MutexLocker lock(mmap_patch_lock);
        // Retry failed patches on a new recording instead of silently forgetting
        // their loss counter. Replacing an already installed hook is idempotent.
        mmap_patched_libs = 0;
        running = true;
    }
    installHooks();
    return Error::OK;
}
void MmapTracer::stop() {
    {
        MutexLocker lock(mmap_patch_lock);
        pthread_mutex_lock(&mmap_mutex);
        running = false;
        pthread_mutex_unlock(&mmap_mutex);
    }
    if (transforming.exchange(false)) retransform_jna();
    if (lost) Log::warn("mmap: %llu operations lost or unsupported; leak report is incomplete", lost.load());
}
bool MmapTracer::transform(jvmtiEnv* jvmti, const char* name, jint len, const u8* data, jint* outlen, u8** out) {
    if (!transforming || !name || strcmp(name, "com/sun/jna/Function") != 0) return false;
    std::vector<u8> result;
    if (!rewriteMmapClass(data, len, result) || jvmti->Allocate(result.size(), out) != JVMTI_ERROR_NONE) {
        mmap_transform_failed = true;
        lost++;
        Log::warn("mmap: unsupported JNA Function bytecode or transformation allocation failure");
    } else {
        memcpy(*out, result.data(), result.size());
        *outlen = result.size();
    }
    return true;
}
