#include "threadNames.h"
#include "profiler.h"

ThreadNames ThreadNames::_instance;

void ThreadNames::update(jthread thread) {
    MutexLocker ml(_lock);
    if (!VMThread::available()) {
        return;
    }
    JNIEnv *env = VM::jni();
    if (_eetop == NULL) {
        _threadClass = env->FindClass("java/lang/Thread");
        if (_threadClass == NULL) {
            return;
        }
        _eetop = env->GetFieldID(_threadClass, "eetop", "J");
        if (_eetop == NULL) {
            return;
        }
    }
    VMThread *vm_thread = (VMThread *)(uintptr_t)env->GetLongField(thread, _eetop);
    jvmtiThreadInfo info;
    jvmtiEnv *jvmti = VM::jvmti();
    if (jvmti->GetThreadInfo(thread, &info) != 0 || info.name == NULL) {
        return;
    }
    _storage[vm_thread->osThreadId()] = std::string(info.name);
    jvmti->Deallocate((unsigned char *)info.name);
}

NamesMap ThreadNames::getNames() {
    MutexLocker ml(_lock);
    std::map<int, std::string> result = _storage;
    return result;
}

void updateAllKnownThreads() {
    jvmtiEnv *jvmti = VM::jvmti();
    jthread *thread_objects;
    int _thread_count;
    if (jvmti->GetAllThreads(&_thread_count, &thread_objects) != 0) {
        return;
    }
    for (int i = 0; i < _thread_count; i++) {
        ThreadNames::_instance.update(thread_objects[i]);
    }
}
