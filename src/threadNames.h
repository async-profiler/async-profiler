#ifndef _THREADNAME_H
#define _THREADNAME_H

#include <jvmti.h>
#include <pthread.h>
#include <map>
#include <string>
#include "vmStructs.h"
#include "vmEntry.h"
#include "concurrencyUtil.h"


typedef std::map<int, std::string> NamesMap;

class ThreadNames {
  private:
    NamesMap _storage;
    pthread_mutex_t _storage_lock;
    jfieldID _cached_eetop;
    pthread_mutex_t _cached_eetop_lock;
    jfieldID _get_eetop(JNIEnv *env);

  public:
    ThreadNames() :
        _cached_eetop(NULL) {
        initLock(&_storage_lock);
        initLock(&_cached_eetop_lock);
    }

    void update(jthread thread);
    void updateAllKnownThreads();
    NamesMap getNames();
};


#endif //_THREADNAME_H
