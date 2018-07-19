#include "threadNames.h"
#include "profiler.h"


jfieldID ThreadNames::_get_eetop(JNIEnv *env) {
    MutexLocker ml(_cached_eetop_lock);
    if (_cached_eetop == NULL) {
        jclass threadClass = env->FindClass("java/lang/Thread");
        if (threadClass != NULL) {
            _cached_eetop = env->GetFieldID(threadClass, "eetop", "J");
        }
    }
    return _cached_eetop;
}

void ThreadNames::update(jthread thread) {
    if (!VMThread::available()) {
        return;
    }
    JNIEnv *env = VM::jni();
    jfieldID eetop = _get_eetop(env);
    if (eetop == NULL) {
        return;
    }
    VMThread *vm_thread = (VMThread *)(uintptr_t)env->GetLongField(thread, eetop);
    jvmtiThreadInfo info;
    jvmtiEnv *jvmti = VM::jvmti();
    if (jvmti->GetThreadInfo(thread, &info) != 0 || info.name == NULL) {
        return;
    }
    MutexLocker ml(_storage_lock);
    _storage[vm_thread->osThreadId()] = std::string(info.name);
    jvmti->Deallocate((unsigned char *)info.name);
}

void ThreadNames::updateAllKnownThreads() {
    jvmtiEnv *jvmti = VM::jvmti();
    jthread *thread_objects;
    int thread_count;
    if (jvmti->GetAllThreads(&thread_count, &thread_objects) != 0) {
        return;
    }
    for (int i = 0; i < thread_count; i++) {
        update(thread_objects[i]);
    }
}

NamesMap ThreadNames::getNames() {
    MutexLocker ml(_storage_lock);
    NamesMap result = _storage;
    return result;
}
