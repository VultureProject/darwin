#pragma once
#include <Python.h>
#include <string>

struct DarwinResponse{
    std::list<unsigned int> certitudes;
    std::string body;
};

extern "C" {

    PyObject* parse_body(PyObject* module, const std::string&);
    PyObject* filter_pre_process(PyObject* module, PyObject*);
    PyObject* filter_process(PyObject* module, PyObject*);
    std::list<std::string> alert_formating(PyObject* module, PyObject*input);
    DarwinResponse output_formating(PyObject* module, PyObject*);
    DarwinResponse response_formating(PyObject* module, PyObject*);

}