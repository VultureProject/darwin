#pragma once
#include <Python.h>
#include <string>

extern "C" {

    PyObject* parse_body(const std::string&);
    PyObject* filter_pre_process(PyObject*);
    PyObject* filter_process(PyObject*);
    std::string alert_formating(PyObject*);
    std::string output_formating(PyObject*);
    std::string response_formating(PyObject*);

}