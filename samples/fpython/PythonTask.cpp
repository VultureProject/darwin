/// \file     PythonExample.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string.h>

#include "../toolkit/rapidjson/document.h"
#include "PythonTask.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"

PythonTask::PythonTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                         std::mutex& cache_mutex, FunctionHolder& functions)
        : Session{DARWIN_FILTER_NAME, socket, manager, cache, cache_mutex}, _functions{functions}
{
    _is_cache = _cache != nullptr;
}

long PythonTask::GetFilterCode() noexcept{
    return DARWIN_FILTER_PYTHON_EXAMPLE;
}

bool PythonTask::ParseLine(rapidjson::Value& line __attribute((unused))) {
    return true;
}

void PythonTask::operator()() {
    DARWIN_LOGGER;
    if(PyErr_Occurred() != nullptr) PyErr_Print();

    PyObject* py_result2 = nullptr;
    if ((py_result2 = PyObject_CallFunctionObjArgs(_functions.preProcessingFunc.py, _result, nullptr)) == nullptr) {
        DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
        if(PyErr_Occurred() != nullptr) PyErr_Print();
    }

    if ((py_result2 = PyObject_CallFunctionObjArgs(_functions.processingFunc.py, py_result2, nullptr)) == nullptr) {
        DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
        if(PyErr_Occurred() != nullptr) PyErr_Print();
    }
    PyObject* preProc_res = nullptr, *proc_res = nullptr;

    switch(_functions.preProcessingOrigin) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((preProc_res = PyObject_CallFunctionObjArgs(_functions.preProcessingFunc.py, _parsed_body, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((preProc_res = _functions.preProcessingFunc.so(_parsed_body)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    switch(_functions.preProcessingOrigin) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((proc_res = PyObject_CallFunctionObjArgs(_functions.processingFunc.py, preProc_res, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((proc_res = _functions.processingFunc.so(preProc_res)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    std::string alert = GetFormated(_functions.alertFormatingFunc, _functions.alertFormatingOrigin, proc_res);
    std::string output = GetFormated(_functions.outputFormatingFunc, _functions.outputFormatingOrigin, proc_res);
    std::string resp = GetFormated(_functions.responseFormatingFunc, _functions.responseFormatingOrigin, proc_res);
    if( ! alert.empty()) {
        DARWIN_ALERT_MANAGER.Alert(alert);
    }

    if( ! output.empty()) {
        _raw_body = output;
    }

    if( ! resp.empty()){
        _response_body = resp;
    }
}

bool PythonTask::ParseBody() {
    DARWIN_LOGGER;
    PyObject* raw_body = nullptr;
    bool ret = false;
    switch(_functions.parseBodyOrigin){
        case FunctionOrigin::NONE:{
            ret = Session::ParseBody();
            if(!ret) 
                return false;
            std::string parsed_body = JsonStringify(_body);
            if(parsed_body.empty()) 
                return true;

            _parsed_body = PyUnicode_DecodeFSDefault(parsed_body.c_str());
            if(PyErr_Occurred() != nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while raw body:"+_raw_body);
                PyErr_Print();
                return false;
            }
            return ret;
        }
        case FunctionOrigin::PYTHON_MODULE:{
            if ((_parsed_body = PyObject_CallFunctionObjArgs(_functions.parseBodyFunc.py, raw_body, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return false;
            }
            return true;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if((_parsed_body = _functions.parseBodyFunc.so(_raw_body))== nullptr){
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the SO function");
                return false;
            }  
            return true;
        }

    }
    return false;
}


std::string PythonTask::GetFormated(FunctionUnion<FunctionHolder::format_t>& func, FunctionOrigin& origin, PyObject* processedData){
    DARWIN_LOGGER;
    PyObject *ret = nullptr;
    char * out = nullptr;
    switch(origin) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((ret = PyObject_CallFunctionObjArgs(func.py, processedData, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return "";
            }
            if((out = PyBytes_AsString(ret)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return "";
            }
            return std::string(out);
        }
        case FunctionOrigin::SHARED_LIBRARY:
            return func.so(processedData);
        default:
            return "";
    }

    
}