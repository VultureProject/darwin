#pragma once
#include <Python.h>

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

private:
    void Decref(){
        if(handle != nullptr){
            if(PyGILState_Check() == 1){
                Py_DECREF(handle);
            } else {
                PythonLock lock;
                Py_DECREF(handle);
            }
            handle = nullptr;
        }
    }
    PyObject* handle;
};