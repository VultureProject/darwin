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
#include "Stats.hpp"

PythonTask::PythonTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                        std::mutex& cache_mutex,
                        darwin::session_ptr_t s,
                        darwin::DarwinPacket& packet, PyObject* pClass, FunctionHolder& functions)
        : ATask{DARWIN_FILTER_NAME, cache, cache_mutex, s, packet}, _pClass{pClass}, _functions{functions}
{
    _is_cache = _cache != nullptr;
    PythonLock lock;
    _pSelf = PyObject_CallObject(_pClass, nullptr);
    Generator::PyExceptionCheckAndLog("PythonTask:: PythonTask:: Error creating Execution object: ", darwin::logger::Error);
}

long PythonTask::GetFilterCode() noexcept{
    return DARWIN_FILTER_PYTHON;
}

bool PythonTask::ParseLine(rapidjson::Value& line __attribute((unused))) {
    return true;
}

void PythonTask::operator()() {
    DARWIN_LOGGER;
    PyObjectOwner preProc_res, proc_res, context_res, alert_context_res;
    if (*_pSelf == nullptr) {
        DARWIN_LOG_ERROR("No Execution object was created, Lines skipped.");
        return;
    }
    switch(_functions.preProcessingFunc.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;
            if ((preProc_res = PyObject_CallFunctionObjArgs(_functions.preProcessingFunc.py, *_pSelf, *_parsed_body, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Preprocess: ", darwin::logger::Error);
                return;
            }
            break;
        }
        case FunctionOrigin::shared_library:{
            try {
                if ((preProc_res = _functions.preProcessingFunc.so(_pClass, *_pSelf, *_parsed_body)) == nullptr) {
                    DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO filter_pre_process function");
                    return;
                }
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: Error while calling the SO filter_pre_process function : " + std::string(e.what()));
                return;
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for preProcessing Origin");
    }

    switch(_functions.processingFunc.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;
            if ((proc_res = PyObject_CallFunctionObjArgs(_functions.processingFunc.py, *_pSelf, *preProc_res, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Process: ", darwin::logger::Error);
                return;
            }
            break;
        }
        case FunctionOrigin::shared_library:{
            try{
                if ((proc_res = _functions.processingFunc.so(_pClass, *_pSelf, *preProc_res)) == nullptr) {
                    DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO filter_process function");
                    return;
                }
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: Error while calling the SO filter_process function : " + std::string(e.what()));
                return;
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for processing Origin");
    }

    switch(_functions.contextualizeFunc.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;
            if ((context_res = PyObject_CallFunctionObjArgs(_functions.contextualizeFunc.py, *_pSelf, *proc_res, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Contextualize: ", darwin::logger::Error);
                return;
            }
            break;
        }
        case FunctionOrigin::shared_library:{
            try{
                if ((context_res = _functions.contextualizeFunc.so(_pClass, *_pSelf, *proc_res)) == nullptr) {
                    DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO filter_contextualize function");
                    return;
                }
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: Error while calling the SO filter_contextualize function : " + std::string(e.what()));
                return;
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for processing Origin");
    }

    switch(_functions.alertContextualizeFunc.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;
            if ((alert_context_res = PyObject_CallFunctionObjArgs(_functions.alertContextualizeFunc.py, *_pSelf, *context_res, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: Operator:: Alert Contextualize: ", darwin::logger::Error);
                return;
            }
            break;
        }
        case FunctionOrigin::shared_library:{
            try{
                if ((alert_context_res = _functions.alertContextualizeFunc.so(_pClass, *_pSelf, *context_res)) == nullptr) {
                    DARWIN_LOG_DEBUG("PythonTask:: An error occurred while calling the SO alert_contextualize function");
                    return;
                }
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: Error while calling the SO alert_contextualize function : " + std::string(e.what()));
                return;
            }
            break;
        }
        default:
            DARWIN_LOG_CRITICAL("PythonTask:: Corrupted state for processing Origin");
    }


    std::vector<std::string> alerts = GetFormatedAlerts(_functions.alertFormatingFunc, *alert_context_res);
    // DarwinResponse output = GetFormatedResponse(_functions.outputFormatingFunc, *context_res);
    DarwinResponse resp = GetFormatedResponse(_functions.responseFormatingFunc, *context_res);

    for(auto& alert: alerts) {
        if( ! alert.empty()) {
            DARWIN_ALERT_MANAGER.Alert(alert);
        }
    }
    for(auto cert: resp.certitudes) {
        if(cert > _threshold) {
            STAT_MATCH_INC;
        }
        _packet.AddCertitude(cert);
    }

    if( ! resp.body.empty()){
        _response_body.clear();
        _response_body.append(resp.body);
    }
}

bool PythonTask::ParseBody() {
    DARWIN_LOGGER;
    PyObjectOwner raw_body;

    if (*_pSelf == nullptr) {
        DARWIN_LOG_ERROR("No Execution object was created, Lines skipped.");
        return false;
    }

    bool ret = false;
    switch(_functions.parseBodyFunc.origin){
        case FunctionOrigin::none:{
            ret = ATask::ParseBody();
            if(!ret){
                return false;
            }
            PythonLock pylock;
            if((_parsed_body = PyList_New(_body.Size())) == nullptr){
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :", darwin::logger::Error);
                return false;
            }

            for(ssize_t i = 0; i < _body.Size(); i++){
                PyObject *str; // This reference is "stolen" by PyList_SetItem, we don't have to DECREF it
                if(! _body.GetArray()[i].IsString()){
                    DARWIN_LOG_ERROR("PythonTask::ParseBody: Item on the list is not a string : " + std::to_string(i));
                    return false;
                }
                if((str = PyUnicode_DecodeFSDefault(_body.GetArray()[i].GetString())) == nullptr) {
                    DARWIN_LOG_ERROR("PythonTask:: ParseBody:: An error occurred while raw body:"+_packet.GetBody());
                    Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :", darwin::logger::Error);
                    return false;
                }
                if(PyList_SetItem(*_parsed_body, i, str) != 0){
                    Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :", darwin::logger::Error);
                    return false;
                }
            }
            return ret;
        }
        case FunctionOrigin::python_module:{
            // This lock is valid because we use only ONE interpreter
            // If at some point, we decide to use multiple interpreters
            // (one by thread for example)
            // We must switch to PythonThread utility class
            PythonLock pylock;
            if((raw_body = PyUnicode_DecodeFSDefault(_packet.GetBody().c_str())) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :", darwin::logger::Error);
                return false;
            }

            if ((_parsed_body = PyObject_CallFunctionObjArgs(_functions.parseBodyFunc.py, *_pSelf, *raw_body, nullptr)) == nullptr) {
                DARWIN_LOG_ERROR("PythonTask:: ParseBody:: An error occurred while calling the Python function");
                Generator::PyExceptionCheckAndLog("PythonTask:: ParseBody:: An error occurred while parsing body :", darwin::logger::Error);
                return false;
            }

            return true;
        }
        case FunctionOrigin::shared_library:{
            try{
                if((_parsed_body = _functions.parseBodyFunc.so(_pClass, *_pSelf, _packet.GetBody()))== nullptr){
                    DARWIN_LOG_ERROR("PythonTask:: ParseBody:: An error occurred while calling the SO function");
                    return false;
                }  
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: ParseBody:: Error while calling the SO parse_body function : " + std::string(e.what()));
                return false;
            }

            return true;
        }

    }
    return false;
}


std::vector<std::string> PythonTask::GetFormatedAlerts(FunctionPySo<FunctionHolder::alert_format_t>& func, PyObject* processedData){
    DARWIN_LOGGER;
    ssize_t pySize = 0, strSize=0;
    const char * out = nullptr;
    std::vector<std::string> ret;
    switch(func.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;
            PyObjectOwner pyRes;
            if ((pyRes = PyObject_CallFunctionObjArgs(func.py, *_pSelf, processedData, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedAlerts:: ", darwin::logger::Error);
                return ret;
            }
            if(PyList_Check(*pyRes) == 0){
                DARWIN_LOG_ERROR("PythonTask::GetFormatedAlerts : 'format_alert' did not return a list");
                return ret;
            }
            pySize = PyList_Size(*pyRes);
            if(Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedAlerts:: ", darwin::logger::Error)) {
                return ret;
            }

            for(ssize_t i = 0; i < pySize; i++) {
                // str is a borrowed reference, we don't have to DECREF it
                PyObject* str = PyList_GetItem(*pyRes, i);
                if(Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedAlerts:: ", darwin::logger::Error)){
                    continue;
                }
                out = PyUnicode_AsUTF8AndSize(str, &strSize);
                if(Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedAlerts:: ", darwin::logger::Error)){
                    continue;
                }
                ret.push_back(std::string(out, strSize));
            }
            return ret;
        }
        case FunctionOrigin::shared_library:{
            try{
                return func.so(_pClass, *_pSelf, processedData);
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: GetFormatedAlerts:: Error while calling the SO alert_format function : " + std::string(e.what()));
                return ret;
            }
        }

        default:
            return ret;
    }

    
}

DarwinResponse PythonTask::GetFormatedResponse(FunctionPySo<FunctionHolder::resp_format_t>& func, PyObject* processedData) {
    DARWIN_LOGGER;
    DarwinResponse ret;
    switch(func.origin) {
        case FunctionOrigin::python_module:{
            PythonLock pylock;

            PyObjectOwner pBody, pCertitudes, pyRes;
            ssize_t certSize = 0;
            if ((pyRes = PyObject_CallFunctionObjArgs(func.py, *_pSelf, processedData, nullptr)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: ", darwin::logger::Error);
                return ret;
            }
            if((pBody = PyObject_GetAttrString(*pyRes, "body")) == nullptr){
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: data.body :", darwin::logger::Error);
                return ret;
            }

            const char* cBody = nullptr;
            ssize_t bodySize = 0;

            if((cBody = PyUnicode_AsUTF8AndSize(*pBody, &bodySize)) == nullptr) {
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: str(data.body) :", darwin::logger::Error);
                return ret;
            }

            ret.body = std::string(cBody, bodySize);
            if((pCertitudes = PyObject_GetAttrString(*pyRes, "certitudes")) == nullptr){
                Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: data.certitudes :", darwin::logger::Error);
                return ret;
            }

            if(PyList_Check(*pCertitudes) == 0){
                DARWIN_LOG_ERROR("PythonTask::GetFormatedResponse : 'data.certitudes' is not a list");
                return ret;
            }

            certSize = PyList_Size(*pCertitudes);
            if(Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: ", darwin::logger::Error)) {
                return ret;
            }

            for(ssize_t i = 0; i < certSize; i++) {
                PyObject* cert = nullptr; // cert is a borrowed reference, no need to DECREF it
                if((cert = PyList_GetItem(*pCertitudes, i)) == nullptr) {
                    Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: ", darwin::logger::Error);
                    continue;
                }

                if(PyLong_Check(cert) == 0) {
                    DARWIN_LOG_ERROR("PythonTask:: GetFormatedResponse:: an object from the 'certitudes' list is not a long");
                    continue;
                }
                long lCert = PyLong_AS_LONG(cert);
                if(Generator::PyExceptionCheckAndLog("PythonTask:: GetFormatedResponse:: ", darwin::logger::Error)) {
                    continue;
                }
                if (lCert < 0 || lCert > UINT_MAX) {
                    DARWIN_LOG_DEBUG("PythonTask:: GetFormatedResponse:: out of range certitude");
                    continue;
                }
                ret.certitudes.push_back(static_cast<unsigned int>(lCert));
            }
            return ret;
        }
        case FunctionOrigin::shared_library:{
            try {
                return func.so(_pClass, *_pSelf, processedData);
            } catch(std::exception const& e){
                DARWIN_LOG_ERROR("PythonTask:: GetFormatedResponse:: Error while calling the SO response_format function : " + std::string(e.what()));
                return ret;
            }

        }
        default:
            return ret;
    }
}

PythonTask::~PythonTask() {
    /**
     * The next lines are not necessaray as exception in destructors are normally ignored by the python interpreter
     * Just in case they are not we may avoid crashes
     */
    PythonLock lock;
    _pSelf.Decref();
    Generator::PyExceptionCheckAndLog("PythonTask::~PythonTask : Erreur while calling 'Execution' destructor", darwin::logger::Error);
}