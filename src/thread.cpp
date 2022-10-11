#include <time.h>
#include "os.h"
#include "thread.h"

static SigAction old_handler;

pthread_key_t ProfiledThread::_tls_key;
int ProfiledThread::_buffer_size = 0;
std::atomic<int> ProfiledThread::_running_buffer_pos(0);
std::vector<ProfiledThread*> ProfiledThread::_buffer;

void ProfiledThread::initTLSKey() {
    static pthread_once_t tls_initialized = PTHREAD_ONCE_INIT;
    pthread_once(&tls_initialized, doInitTLSKey);
}

void ProfiledThread::doInitTLSKey() {
    pthread_key_create(&_tls_key, freeKey);
}

inline void ProfiledThread::freeKey(void *key) {
    ProfiledThread* tls_ref = (ProfiledThread*)(key);
    if (tls_ref != NULL) {
        delete tls_ref;
    }
}

void ProfiledThread::initCurrentThread() {
    initTLSKey();
    
    ProfiledThread* tls = (ProfiledThread*)pthread_getspecific(_tls_key);
    if (tls == NULL) {
        int tid = OS::threadId();
        tls = ProfiledThread::forTid(tid);
        pthread_setspecific(_tls_key, (const void*) tls);
    }
}

void ProfiledThread::initExistingThreads() {
    static pthread_once_t initialized = PTHREAD_ONCE_INIT;
    pthread_once(&initialized, doInitExistingThreads);
}

void ProfiledThread::initCurrentThreadWithBuffer() {
    initTLSKey();

    if (pthread_getspecific(_tls_key) != NULL) {
        // if there is already a TLS value associated just bail out
        return;
    }

    ProfiledThread* tls_ref = NULL;
    int pos = _running_buffer_pos++;
    if (pos < _buffer_size) {
        tls_ref = _buffer[pos];
        tls_ref->_tid = OS::threadId();
    }
    if (tls_ref != NULL) {
        pthread_setspecific(_tls_key, (const void*)tls_ref);
    } else {
        const char* msg = "ProfiledThread TLS buffer too small.";
        Profiler::instance()->writeLog(LOG_WARN, msg, strlen(msg));
    }
}

void* ProfiledThread::delayedUninstallUSR1(void* unused) {
    initTLSKey();

    int res = 0;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000;
    // wait for the TLS to be set
    while ((!res || errno == EINTR) && pthread_getspecific(_tls_key) == NULL) {
        res = nanosleep(&ts, &ts);
    }

    /*
     Wait 5 secs to finish other threads initialization - should be more than enough.
     This is the best we can do - we can not use any synchronization between the signal handler running in threads
     to eg. have a countdown latch and uninstall only when all threads completed the init. In addition to that
     the threads from the initial list can be terminated by the time they should process the signal and we would need
     to synchronize with the thread-end event captured by JVMTI, otherwise we might be waiting forever.

     A fixed timeout approach sounds like an acceptable compromise - in the worst case there will be a few outliers without 
     the TLS instance associated with them and they will have to take the slow path to resolve the thread id and will not
     be able to use anything depending on data stored in that TLS instance.

     In real life, though, the 5 secs timeout should be more than enough.
    */
    ts.tv_sec = 5;
    ts.tv_nsec = 0;
    do { 
        res = nanosleep(&ts, &ts); 
    } while (res == -1 && errno == EINTR);
    // now remove the TLS init signal handler
    OS::installSignalHandler(SIGUSR1, old_handler);
    return NULL;
}

void ProfiledThread::doInitExistingThreads() {
    pthread_t thrd;
    if (pthread_create(&thrd, NULL, delayedUninstallUSR1, NULL) == 0) {
        std::unique_ptr<ThreadList> tlist{OS::listThreads()};
        /*
        Here is a bit of trickery - we need the TLS variable initialized for all existing threads but we can not do this
        from outside. Therefore, we need to install signal handler to perform the initialization. However, the signal handler
        can not allocate - in order to work around that limitation we pre-allocate an array of ProfiledThread instances
        for all existing threads. This array will be used by the threads when handling the signal to pick the pre-allocated instance
        which can be stored in the TLS slot.

        Any newly started threads will be handled by the JVMTI callback so we need to worry only about the existing threads here.
        */
        prepareBuffer(tlist->size());
    
        old_handler = OS::installSignalHandler(SIGUSR1, ProfiledThread::signalHandler);
        int cntr = 0;
        int tid = -1;
        while ((tid = tlist->next()) != -1) {
            if (tlist->size() <= cntr++) {
                break;
            }
            OS::sendSignalToThread(tid, SIGUSR1);
        }
        pthread_detach(thrd);
    }
}

void ProfiledThread::prepareBuffer(int size) {
    Log::debug("Initializing ProfiledThread TLS buffer to %d slots", size);

    _running_buffer_pos = 0;
    _buffer_size = size;
    _buffer.reserve(size);
    for (int i = 0; i < size; i++) {
        _buffer.push_back(ProfiledThread::inBuffer(i));
    }
}

void ProfiledThread::release() {
    pthread_key_t key = _tls_key;
    if (key == 0) {
        return;
    }
    ProfiledThread* tls = (ProfiledThread*)pthread_getspecific(key);
    if (tls != NULL) {
        tls->releaseFromBuffer();
        delete tls;
        pthread_setspecific(key, NULL);
    }
}

void ProfiledThread::releaseFromBuffer() {
    if (_buffer_pos >= 0) {
        _buffer[_buffer_pos] = NULL;
        _buffer_pos = -1;
    }
}

bool ProfiledThread::noteWallSample(bool all, u64* skipped_samples) {
    if (all) {
        _wall_epoch = _cpu_epoch;
        *skipped_samples = 0;
        return true;
    }

    if (_wall_epoch == _cpu_epoch) {
        *skipped_samples = ++_skipped_samples;
        return false;
    }
    _wall_epoch = _cpu_epoch;
    *skipped_samples = _skipped_samples;
    _skipped_samples = 0;
    return true;
}

inline ProfiledThread* ProfiledThread::current() {
    pthread_key_t key = _tls_key;
    return key != 0 ? (ProfiledThread*) pthread_getspecific(key) : NULL;
}

void ProfiledThread::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (signo == SIGUSR1) {
        initCurrentThreadWithBuffer();
    }
}