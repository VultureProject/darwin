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
                         std::mutex& cache_mutex, PyObject* pModule, FunctionHolder& functions)
        : Session{DARWIN_FILTER_NAME, socket, manager, cache, cache_mutex}, _pModule{pModule}, _functions{functions}
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
            if ((preProc_res = _functions.preProcessingFunc.f.so(_pModule, _parsed_body)) == nullptr) {
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
            if ((proc_res = _functions.processingFunc.f.so(_pModule, preProc_res)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    std::list<std::string> alerts = GetFormatedAlerts(_functions.alertFormatingFunc, proc_res);
    DarwinResponse output = GetFormatedResponse(_functions.outputFormatingFunc, proc_res);
    DarwinResponse resp = GetFormatedResponse(_functions.responseFormatingFunc, proc_res);
    Py_DECREF(proc_res);
    Py_DECREF(preProc_res);

    for(auto& alert: alerts) {
        if( ! alert.empty()) {
            DARWIN_ALERT_MANAGER.Alert(alert);
        }
    }
    for(auto cert: resp.certitudes) {
        _certitudes.push_back(cert);
    }
    if( ! output.body.empty()) {
        _raw_body = output.body;
    }

    if( ! resp.body.empty()){
        _response_body = resp.body;
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
            if((_parsed_body = _functions.parseBodyFunc.f.so(_pModule, _raw_body))== nullptr){
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the SO function");
                return false;
            }  
            return true;
        }

    }
    return false;
}


std::list<std::string> PythonTask::GetFormatedAlerts(FunctionPySo<FunctionHolder::alert_format_t>& func, PyObject* processedData){
    DARWIN_LOGGER;
    PyObject *pyRes = nullptr;
    ssize_t pySize = 0, strSize=0;
    const char * out = nullptr;
    std::list<std::string> ret;
    switch(func.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            if ((pyRes = PyObject_CallFunctionObjArgs(func.f.py, processedData, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return ret;
            }
            if(PyList_Check(pyRes) == 0){
                DARWIN_LOG_ERROR("PythonTask::GetFormated : not a list");
            }
            pySize = PyList_Size(pyRes);
            if(PyErr_Occurred() != nullptr) {
                PyErr_Print();
                return ret;
            }

            for(ssize_t i = 0; i < pySize; i++) {
                PyObject* str = PyList_GetItem(pyRes, i);
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                out = PyUnicode_AsUTF8AndSize(str, &strSize);
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                ret.push_back(std::string(out, strSize));
            }
            return ret;
        }
        case FunctionOrigin::SHARED_LIBRARY:
            return func.f.so(_pModule, processedData);
        default:
            return ret;
    }

    
}

DarwinResponse PythonTask::GetFormatedResponse(FunctionPySo<FunctionHolder::resp_format_t>& func, PyObject* processedData) {
    DARWIN_LOGGER;
    DarwinResponse ret;
    switch(func.loc) {
        case FunctionOrigin::PYTHON_MODULE:{

            PyObject * pBody = nullptr, *pCertitudes = nullptr;
            ssize_t certSize = 0;
            if((pBody = PyObject_GetAttrString(processedData, "response")) == nullptr){
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return ret;
            }

            const char* cBody = nullptr;
            ssize_t bodySize = 0;

            if((cBody = PyUnicode_AsUTF8AndSize(pBody, &bodySize)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return ret;
            }

            ret.body = std::string(cBody, bodySize);
            Py_DECREF(pBody);

            if((pCertitudes = PyObject_GetAttrString(processedData, "certitudes")) == nullptr){
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the Python function");
                if(PyErr_Occurred() != nullptr) PyErr_Print();
                return ret;
            }
            certSize = PyList_Size(pCertitudes);
            if(PyErr_Occurred() != nullptr) {
                PyErr_Print();
                return ret;
            }

            for(ssize_t i = 0; i < certSize; i++) {
                PyObject* cert = nullptr;
                if((cert = PyList_GetItem(pCertitudes, i)) == nullptr) {
                    if(PyErr_Occurred() != nullptr) PyErr_Print();
                    continue;
                }

                if(PyLong_Check(cert) == 0) {
                    DARWIN_LOG_DEBUG("PythonTask:: Not a long");
                }
                long lCert = PyLong_AS_LONG(cert);
                if(PyErr_Occurred() != nullptr){
                    DARWIN_LOG_DEBUG("PythonTask:: error getting long");
                    PyErr_Print();
                    continue;
                }
                if (lCert < 0 || lCert > UINT_MAX) {
                    DARWIN_LOG_DEBUG("PythonTask:: out of range certitude");
                    continue;
                }
                ret.certitudes.push_back(static_cast<unsigned int>(lCert));
            }
            return ret;
        }
        case FunctionOrigin::SHARED_LIBRARY:
            return func.f.so(_pModule, processedData);
        default:
            return ret;
    }    
}