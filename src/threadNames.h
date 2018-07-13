#ifndef _THREADNAME_H
#define _THREADNAME_H

#include <jvmti.h>
#include <pthread.h>
#include <map>
#include "vmStructs.h"
#include "vmEntry.h"


typedef std::map<int, std::string> NamesMap;

class ThreadNames {
  private:
    pthread_mutex_t _lock;
    NamesMap _storage;
    jclass _threadClass;
    jfieldID _eetop;
  public:
    static ThreadNames _instance;

    ThreadNames() :
        _threadClass(NULL),
        _eetop(NULL) {}

    void update(jthread thread);
    NamesMap getNames();
};

void updateAllKnownThreads();

#endif //_THREADNAME_H
