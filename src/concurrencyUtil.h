#ifndef _CONCURRENCYUTIL_H
#define _CONCURRENCYUTIL_H

#include <pthread.h>


class MutexLocker {
private:
    pthread_mutex_t* _mutex;

public:
    MutexLocker(pthread_mutex_t& mutex) : _mutex(&mutex) {
        pthread_mutex_lock(_mutex);
    }

    ~MutexLocker() {
        pthread_mutex_unlock(_mutex);
    }
};

void initLock(pthread_mutex_t* lock);

#endif //_CONCURRENCYUTIL_H
