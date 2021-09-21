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
#include "PythonObject.hpp"

PythonTask::PythonTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                        std::mutex& cache_mutex,
                        darwin::session_ptr_t s,
                        darwin::DarwinPacket& packet, PyObject* pModule, FunctionHolder& functions)
        : ATask{DARWIN_FILTER_NAME, cache, cache_mutex, s, packet}, _pModule{pModule}, _functions{functions}
{
    _is_cache = _cache != nullptr;
}

long PythonTask::GetFilterCode() noexcept{
    return DARWIN_FILTER_PYTHON;
}

bool PythonTask::ParseLine(rapidjson::Value& line __attribute((unused))) {
    return true;
}



void PythonTask::operator()() {
    DARWIN_LOGGER;
    PyObject* preProc_res = nullptr, *proc_res = nullptr;

    switch(_functions.preProcessingFunc.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            PythonLock pylock;
            if ((preProc_res = PyObject_CallFunctionObjArgs(_functions.preProcessingFunc.f.py, _parsed_body, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Preprocess: ");
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((preProc_res = _functions.preProcessingFunc.f.so(_pModule, _parsed_body)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO pre-process function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    switch(_functions.processingFunc.loc) {
        case FunctionOrigin::PYTHON_MODULE:{
            PythonLock pylock;
            if ((proc_res = PyObject_CallFunctionObjArgs(_functions.processingFunc.f.py, preProc_res, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Process: ");
            }
            break;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            if ((proc_res = _functions.processingFunc.f.so(_pModule, preProc_res)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO process function");
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for processing Origin");
    }

    std::list<std::string> alerts = GetFormatedAlerts(_functions.alertFormatingFunc, proc_res);
    DarwinResponse output = GetFormatedResponse(_functions.outputFormatingFunc, proc_res);
    DarwinResponse resp = GetFormatedResponse(_functions.responseFormatingFunc, proc_res);
    {
        PythonLock pylock;
        Py_DECREF(proc_res);
        Py_DECREF(preProc_res);
    }

    for(auto& alert: alerts) {
        if( ! alert.empty()) {
            DARWIN_ALERT_MANAGER.Alert(alert);
        }
    }
    for(auto cert: resp.certitudes) {
        DARWIN_LOG_DEBUG("PythonTask:: cert added");
        _packet.AddCertitude(cert);
    }
    std::string& body = _packet.GetMutableBody();
    if( ! output.body.empty()) {
        DARWIN_LOG_DEBUG("PythonTask:: output");
        body.clear();
        body.append(output.body);
    }

    if( ! resp.body.empty()){
        DARWIN_LOG_DEBUG("PythonTask:: resp");
        body.clear();
        body.append(resp.body);
    }
}

bool PythonTask::ParseBody() {
    DARWIN_LOGGER;
    PyObjectOwner raw_body;
    bool ret = false;
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: helo");

    switch(_functions.parseBodyFunc.loc){
        case FunctionOrigin::NONE:{
            DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: None koi " "__LINE__");
            ret = ATask::ParseBody();
            DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: None koi2");
            if(!ret){
                return false;
            }
            PythonLock pylock;
            if((_parsed_body = PyList_New(_body.Size())) == nullptr){
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :");
                return false;
            }

            for(ssize_t i = 0; i < _body.Size(); i++){
                PyObject *str;
                if((str = PyUnicode_DecodeFSDefault(_body.GetArray()[i].GetString())) == nullptr) {
                    DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while raw body:"+_packet.GetBody());
                    Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :");
                    return false;
                }
                PyList_SetItem(_parsed_body, i, str);
                if(Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :")){
                    return false;
                }
            }
            return ret;
        }
        case FunctionOrigin::PYTHON_MODULE:{
            DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: koi");
            // This lock is valid because we use only ONE interpreter
            // If at some point, we decide to use multiple interpreters
            // (one by thread for example)
            // We must switch to PythonThread utility class
            PythonLock pylock;
            DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: koilol");
            if((raw_body = PyUnicode_DecodeFSDefault(_packet.GetBody().c_str())) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :");
                return false;
            }
                        std::cout << std::endl << "after unicode decode" << std::endl;

                        DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: koilol yess");

            if ((_parsed_body = PyObject_CallFunctionObjArgs(_functions.parseBodyFunc.f.py, *raw_body, nullptr)) == nullptr) {
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the Python function");
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :");
                return false;
            }
            
            return true;
        }
        case FunctionOrigin::SHARED_LIBRARY:{
            DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: SO koi " "__LINE__");
            try{
                if((_parsed_body = _functions.parseBodyFunc.f.so(_pModule, _packet.GetBody()))== nullptr){
                    DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: An error occurred while calling the SO function");
                    return false;
                }  
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: ParseBody:: Error while calling the SO parse_body function : " + std::string(e.what()));
                return false;
            }
                DARWIN_LOG_DEBUG("PythonTask:: ParseBody:: SO koi 2" "__LINE__");

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
            PythonLock pylock;
            if ((pyRes = PyObject_CallFunctionObjArgs(func.f.py, processedData, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedAlerts:: ");
                return ret;
            }
            if(PyList_Check(pyRes) == 0){
                DARWIN_LOG_ERROR("PythonTask::GetFormatedAlerts : not a list");
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
            DARWIN_LOG_DEBUG("PythonTask::GetFormatedResponse start");
            PythonLock pylock;
            DARWIN_LOG_DEBUG("PythonTask::GetFormatedResponse lock");

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
            DARWIN_LOG_DEBUG("PythonTask::GetFormatedResponse cert");
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
            DARWIN_LOG_DEBUG("PythonTask::GetFormatedResponse regturn");
            return ret;
        }
        case FunctionOrigin::SHARED_LIBRARY:
            return func.f.so(_pModule, processedData);
        default:
            return ret;
    }    
}