#include <Python.h>

class PythonThread {
public:
    PythonThread(): _init{false}, pThreadState{nullptr} {}
    void Init(PyInterpreterState * interp) {
        if (! _init){
            pThreadState = PyThreadState_New(interp);
            _init = true;
        }
    }
    bool IsInit(){
        return _init;
    }

    PyThreadState* GetThreadState(){
        return pThreadState;
    }

    void SetThreadState(PyThreadState* state){
        pThreadState = state;
    }

    ~PythonThread() {
        if(pThreadState != nullptr){
            PyThreadState_Clear(pThreadState);
            PyThreadState_Delete(pThreadState);
        }
    }

    class Use {
    public:
        Use(PythonThread& thread): _thread{thread} {
            PyEval_RestoreThread(_thread.GetThreadState());
        }

        ~Use(){
            _thread.SetThreadState(PyEval_SaveThread());
        }

    private:
        PythonThread& _thread;
    };

private:

    bool _init;
    PyThreadState * pThreadState;

};

class PythonLock {
public:
    PythonLock(){
        lock = PyGILState_Ensure();
    }
    ~PythonLock(){
        PyGILState_Release(lock);
    }
private:
    PyGILState_STATE lock;

};