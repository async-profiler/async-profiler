#include "concurrencyUtil.h"

void initLock(pthread_mutex_t* lock) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(lock, &attr);
}