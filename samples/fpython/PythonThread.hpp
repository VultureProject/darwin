#include <Python.h>

class PythonThread {
public:
    PythonThread(): _init{false}, _pThreadState{nullptr} {}
    void Init(PyInterpreterState * interp) {
        if (! _init){
            _pThreadState = PyThreadState_New(interp);
            _init = true;
        }
    }
    bool IsInit(){
        return _init;
    }

    PyThreadState* GetThreadState(){
        return _pThreadState;
    }

    void SetThreadState(PyThreadState* state){
        _pThreadState = state;
    }

    ~PythonThread() {
        if(_pThreadState != nullptr){
            PyThreadState_Clear(_pThreadState);
            PyThreadState_Delete(_pThreadState);
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
    PyThreadState * _pThreadState;

};

class PythonLock {
public:
    PythonLock(){
        _lock = PyGILState_Ensure();
    }
    ~PythonLock(){
        PyGILState_Release(_lock);
    }
private:
    PyGILState_STATE _lock;

};