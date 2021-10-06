#pragma once
#include <Python.h>

///
/// \brief Helper class that takes the ownership of a PyObject pointer
///        when detroyed, if it still holds a reference, it will attempt to DECREF it to the interpreter
///        Calls to destructor or Move operator or Decref will query the GIL if it does not have it.
///        These methods are always safe to call
///
class PyObjectOwner {
    public:
    // no copy, move ok
    PyObjectOwner(): handle{nullptr} {}
    PyObjectOwner(PyObject* o): handle{o} {}
    ~PyObjectOwner(){
        this->Decref();
    }

    PyObjectOwner(PyObjectOwner const&) = delete;
    PyObjectOwner(PyObjectOwner&& other): handle{other.handle} {
        other.handle = nullptr;
    }

    PyObjectOwner& operator=(PyObjectOwner const&) = delete;
    PyObjectOwner& operator=(PyObjectOwner&& other){
        this->Decref();
        this->handle = other.handle;
        other.handle = nullptr;
        return *this;
    }

    bool operator==(std::nullptr_t){
        return handle == nullptr;
    }

    bool operator!=(std::nullptr_t){
        return handle != nullptr;
    }

    PyObjectOwner& operator=(PyObject* o){
        this->Decref();
        this->handle = o;
        return *this;
    }

    PyObject* operator* ()
    {
        return handle;
    }

    PyObject& operator-> ()
    {
        return *handle;
    }

    void Decref(){
        if(handle != nullptr && Py_IsInitialized() != 0){
            if(PyGILState_Check() == 1){
                Py_DECREF(handle);
            } else {
                PythonLock lock;
                Py_DECREF(handle);
            }
            handle = nullptr;
        }
    }

private:
    PyObject* handle;
};