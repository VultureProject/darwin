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
    PyObject* preProc_res = nullptr, *proc_res = nullptr;

    switch(_functions.preProcessingFunc.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((preProc_res = PyObject_CallFunctionObjArgs(_functions.preProcessingFunc.f.py, _parsed_body, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((preProc_res = _functions.preProcessingFunc.f.so(_parsed_body)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    switch(_functions.processingFunc.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((proc_res = PyObject_CallFunctionObjArgs(_functions.processingFunc.f.py, preProc_res, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((proc_res = _functions.processingFunc.f.so(preProc_res)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    std::string alert = GetFormated(_functions.alertFormatingFunc, proc_res);
    std::string output = GetFormated(_functions.outputFormatingFunc, proc_res);
    std::string resp = GetFormated(_functions.responseFormatingFunc, proc_res);
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
    switch(_functions.parseBodyFunc.loc){
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
            raw_body = PyUnicode_DecodeFSDefault(_raw_body.c_str());
            if((raw_body = PyUnicode_DecodeFSDefault(_raw_body.c_str())) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while raw body:"+_raw_body);
                PyErr_Print();
                return false;
            }
            if ((_parsed_body = PyObject_CallFunctionObjArgs(_functions.parseBodyFunc.f.py, raw_body, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return false;
            }
            return true;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if((_parsed_body = _functions.parseBodyFunc.f.so(_raw_body))== nullptr){
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the SO function");
                return false;
            }  
            return true;
        }

    }
    return false;
}


std::string PythonTask::GetFormated(FunctionPySo<FunctionHolder::format_t>& func, PyObject* processedData){
    DARWIN_LOGGER;
    PyObject *ret = nullptr;
    const char * out = nullptr;
    switch(func.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((ret = PyObject_CallFunctionObjArgs(func.f.py, processedData, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return "";
            }
            if((out = PyUnicode_AS_DATA(ret)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return "";
            }
            return std::string(out);
        }
        case FunctionOrigin::SHARED_LIBRARY:
            return func.f.so(processedData);
        default:
            return "";
    }

    
}