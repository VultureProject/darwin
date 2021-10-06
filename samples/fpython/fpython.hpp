#pragma once
#include <Python.h>
#include "../../toolkit/rapidjson/document.h"

#ifdef __cplusplus
#include <string>
#include <vector>
struct DarwinResponse{
    std::vector<unsigned int> certitudes;
    std::string body;
};
#endif
struct DarwinResponseC{
    unsigned int *certitudes;
    size_t certitudes_size;
    char* body;
    size_t body_size;
};

extern "C" {
    // C++ linkage
#ifdef __cplusplus
    bool filter_config(rapidjson::Document const&);
    PyObject* parse_body(PyObject* module, const std::string&);

    std::vector<std::string> alert_formating(PyObject* module, PyObject*input);
    DarwinResponse output_formating(PyObject* module, PyObject*);
    DarwinResponse response_formating(PyObject* module, PyObject*);
#endif
    // C compatible linkage
    PyObject* filter_pre_process(PyObject* module, PyObject*);
    PyObject* filter_process(PyObject* module, PyObject*);
    PyObject* filter_contextualize(PyObject* module, PyObject*);
    PyObject* alert_contextualize(PyObject* module, PyObject*);


    bool filter_config_c(const char*, const size_t);
    PyObject* parse_body_c(PyObject* module, const char*, const size_t);
    void alert_formating_c(PyObject* module, PyObject* input, char ** alerts, size_t nb_alerts, size_t* alerts_sizes);
    DarwinResponseC* output_formating_c(PyObject* module, PyObject*);
    DarwinResponseC* response_formating_c(PyObject* module, PyObject*);
}