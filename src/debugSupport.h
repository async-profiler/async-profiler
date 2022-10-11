#ifndef _DEBUGSUPPORT_H
#define _DEBUGSUPPORT_H

typedef void (*SetSigHandlerTidRef)(int tid);

class Shims {
  private:
    static Shims _instance;
    SetSigHandlerTidRef _tid_setter_ref;
    Shims();

  public:
    void setSighandlerTid(int tid);
    inline static Shims instance() {
        return _instance;
    }
};

#endif //_DEBUGSUPPORT_H