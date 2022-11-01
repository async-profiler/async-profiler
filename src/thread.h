#ifndef _THREAD_H
#define _THREAD_H

#include <atomic>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <vector>
#include "os.h"

class ProfiledThread {
  private:
    static pthread_key_t _tls_key;
    static int _buffer_size;
    static std::atomic<int> _running_buffer_pos;
    static std::vector<ProfiledThread*> _buffer;

    static void initTLSKey();
    static void doInitTLSKey();
    static inline void freeKey(void *key);
    static void initCurrentThreadWithBuffer();
    static void doInitExistingThreads();
    static void prepareBuffer(int size);
    static void* delayedUninstallUSR1(void* unused);

    int _buffer_pos;
    int _tid;

    ProfiledThread(int buffer_pos, int tid) :  _buffer_pos(buffer_pos), _tid(tid) {};

    void releaseFromBuffer();
  public:
    static ProfiledThread* forTid(int tid) {
        return new ProfiledThread(-1, tid);
    }
    static ProfiledThread* inBuffer( int buffer_pos) {
        return new ProfiledThread(buffer_pos, 0);
    }

    static void initCurrentThread();
    static void initExistingThreads();

    static void release();
    static inline int currentTid() {
        ProfiledThread* tls = current();
        if (tls != NULL) {
            return tls->tid();
        }
        return OS::threadId();
    }

    static inline ProfiledThread* current();

    inline int tid() {
        return _tid;
    }

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
};

#endif // _THREAD_H
