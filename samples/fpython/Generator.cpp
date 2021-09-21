/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <fstream>
#include <dlfcn.h>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "PythonTask.hpp"
#include "ASession.hpp"
#include "AlertManager.hpp"
#include "fpython.hpp"
#include "PythonObject.hpp"

///
/// \brief This expects the thread to hold the GIL 
/// 
/// \param prefix_log 
/// \param log_type 
/// \return true if an exception was found and handled
/// \return false if no exception were risen
///
bool Generator::PyExceptionCheckAndLog(std::string const& prefix_log, darwin::logger::log_type log_type){
    if(PyErr_Occurred() == nullptr){
        return false;
    }
    DARWIN_LOGGER;
    PyObject *errType, *err, *tb;
    PyObjectOwner pErrStr, pTypeStr;
    const char * pErrChars, *pTypeChars;
    ssize_t errCharsLen = 0,  typeCharsLen = 0;
    std::string errString, typeString;
    PyErr_Fetch(&errType, &err, &tb);

    if((pErrStr = PyObject_Str(err)) != nullptr){
        if((pErrChars = PyUnicode_AsUTF8AndSize(*pErrStr, &errCharsLen)) != nullptr) {
            errString = std::string(pErrChars, errCharsLen);
        }
    }

    if((pTypeStr = PyObject_Str(errType)) != nullptr){
        if((pTypeChars = PyUnicode_AsUTF8AndSize(*pTypeStr, &typeCharsLen)) != nullptr) {
            typeString = std::string(pTypeChars, typeCharsLen);
        }
    }        
    std::ostringstream oss;
    oss << prefix_log;
    oss << "Python error '" << typeString << "' : " << errString;
    log.log(log_type, oss.str());
    return true;
}

Generator::Generator(size_t nb_task_threads) 
    : AGenerator(nb_task_threads)
         { }

bool Generator::ConfigureAlerting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Python:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("Python:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Python:: Generator:: Loading configuration...");

    std::string python_script_path, shared_library_path;

    if (!configuration.HasMember("python_script_path")) {
        DARWIN_LOG_CRITICAL("Python:: Generator:: Missing parameter: 'python_script_path'");
        return false;
    }

    if (!configuration["python_script_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Python:: Generator:: 'python_script_path' needs to be a string");
        return false;
    }

    python_script_path = configuration["python_script_path"].GetString();

    if (!configuration.HasMember("shared_library_path")) {
        DARWIN_LOG_CRITICAL("Python:: Generator:: Missing parameter: 'shared_library_path'");
        return false;
    }

    if (!configuration["shared_library_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Python:: Generator:: 'shared_library_path' needs to be a string");
        return false;
    }

    shared_library_path = configuration["shared_library_path"].GetString();
    DARWIN_LOG_DEBUG("Python:: Generator:: Loading configuration...");

    bool pythonLoad = LoadPythonScript(python_script_path);

    if(Py_IsInitialized() != 0) {
        // Release the GIL, This is safe to call because we just initialized the python interpreter
        PyEval_SaveThread();
    }

    return pythonLoad && LoadSharedLibrary(shared_library_path) && CheckConfig() && SendConfig(configuration);
}

bool Generator::SendConfig(rapidjson::Document const& config) {
    DARWIN_LOGGER;
    bool pyConfigRes = false, soConfigRes = false;
    switch(functions.configPyFunc.loc) {
        case FunctionOrigin::NONE:
            DARWIN_LOG_INFO("Python:: Generator:: SendConfig: No configuration function found in python script");
            pyConfigRes = true;
            break;
        case FunctionOrigin::PYTHON_MODULE:
            pyConfigRes = this->SendPythonConfig(config);
            break;
        case FunctionOrigin::SHARED_LIBRARY:
            DARWIN_LOG_CRITICAL("Python:: Generator:: SendConfig: Incoherent configuration");
            return false;
    }

    switch(functions.configSoFunc.loc) {
        case FunctionOrigin::NONE:
            DARWIN_LOG_INFO("Python:: Generator:: SendConfig: No configuration function found in shared object");
            soConfigRes = true;
            break;
        case FunctionOrigin::PYTHON_MODULE:
            DARWIN_LOG_CRITICAL("Python:: Generator:: SendConfig: Incoherent configuration");
            return false;
        case FunctionOrigin::SHARED_LIBRARY:
            soConfigRes = functions.configSoFunc.f.so(config);
            break;
    }

    return pyConfigRes && soConfigRes;
}

bool Generator::SendPythonConfig(rapidjson::Document const& config) {
    DARWIN_LOGGER;
    rapidjson::StringBuffer buffer;
    buffer.Clear();

    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    config.Accept(writer);

    PythonLock lock;
    if(PyRun_SimpleString("import json") != 0) {
        DARWIN_LOG_CRITICAL("Python:: Generator:: SendPythonConfig: import json failed");
        return false;
    }

    std::string cmd = "json.loads(\"\"\"";
    cmd += buffer.GetString();
    cmd += "\"\"\")";
    PyObject* globalDictionary = PyModule_GetDict(*pModule);

    PyObjectOwner pConfig = PyRun_StringFlags(cmd.c_str(), Py_eval_input, globalDictionary, globalDictionary, nullptr);
    if(Generator::PyExceptionCheckAndLog("Python:: Generator:: SendPythonConfig: Converting config to Py Object :")){
        return false;
    }
    PyObjectOwner res;
    if ((res  = PyObject_CallFunctionObjArgs(functions.configPyFunc.f.py, *pConfig, nullptr)) == nullptr) {
        Generator::PyExceptionCheckAndLog("Python:: Generator:: SendPythonConfig: ");
        return false;
    }
    int resTruthiness = -1;
    if((resTruthiness = PyObject_IsTrue(*res)) == 1) {
        // 1 : true
        return true;
    } else if (resTruthiness == 0){
        // 0 : false
        DARWIN_LOG_CRITICAL("Python:: Generator:: SendPythonConfig: filter_config returned false");
    } else {
        // -1 : error while evaluating truthiness
        DARWIN_LOG_CRITICAL("Python:: Generator:: SendPythonConfig: truthiness evaluation of the returned value of 'filter_config' failed");
        Generator::PyExceptionCheckAndLog("Python:: Generator:: SendPythonConfig: ");
    }

    return false;
}

bool Generator::LoadSharedLibrary(const std::string& shared_library_path) {
    DARWIN_LOGGER;

    if(shared_library_path.empty()){
        DARWIN_LOG_INFO("Generator::LoadSharedLibrary : No shared library to load");
        return true;
    }

    void* handle = dlopen(shared_library_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if(handle == nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadSharedLibrary : Error loading the shared library : failed to open " + shared_library_path + " : " + std::string(dlerror()));
        return false;
    }

    FunctionHolder::config_t conf = (FunctionHolder::config_t)dlsym(handle, "filter_config");    

    if(conf == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_config' function in the shared library");
    } else {
        functions.configSoFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.configSoFunc.f.so = conf;
    }

    FunctionHolder::parse_body_t parse = (FunctionHolder::parse_body_t)dlsym(handle, "parse_body");    

    if(parse == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'parse_body' function in the shared library");
    } else {
        functions.parseBodyFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.parseBodyFunc.f.so = parse;
    }

    FunctionHolder::process_t preProc = (FunctionHolder::process_t)dlsym(handle, "filter_pre_process");
    if(preProc == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_pre_process' function in the shared library");
    } else {
        functions.preProcessingFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.preProcessingFunc.f.so = preProc;
    }

    FunctionHolder::process_t proc = (FunctionHolder::process_t)dlsym(handle, "filter_process");
    if(proc == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_process' function in the shared library");
    } else {
        functions.processingFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.processingFunc.f.so = proc;
    }

    FunctionHolder::alert_format_t alert_format = (FunctionHolder::alert_format_t)dlsym(handle, "alert_formating");
    if(alert_format == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'alert_formating' function in the shared library");
    } else {
        functions.alertFormatingFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.alertFormatingFunc.f.so = alert_format;
    }

    FunctionHolder::resp_format_t output_format = (FunctionHolder::resp_format_t)dlsym(handle, "output_formating");
    if(parse == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'output_formating' function in the shared library");
    } else {
        functions.outputFormatingFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.outputFormatingFunc.f.so = output_format;
    }

    FunctionHolder::resp_format_t resp_format = (FunctionHolder::resp_format_t)dlsym(handle, "response_formating");
    if(resp_format == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'response_formating' function in the shared library");
    } else {
        functions.responseFormatingFunc.loc = FunctionOrigin::SHARED_LIBRARY;
        functions.responseFormatingFunc.f.so = resp_format;
    }

    return true;
}

bool Generator::CheckConfig() const {
    if(functions.preProcessingFunc.loc == FunctionOrigin::NONE
        || functions.processingFunc.loc == FunctionOrigin::NONE
        || functions.alertFormatingFunc.loc == FunctionOrigin::NONE
        || functions.outputFormatingFunc.loc == FunctionOrigin::NONE
        || functions.responseFormatingFunc.loc == FunctionOrigin::NONE) 
    {
        DARWIN_LOGGER;
        DARWIN_LOG_CRITICAL("Generator::CheckConfig : Mandatory methods were not found in the python script or the shared library");
        return false;
    }
    return true;
}

PythonThread& Generator::GetPythonThread(){
    thread_local PythonThread thread;
    if(! thread.IsInit()){
        PyGILState_STATE lock = PyGILState_Ensure();
        thread.Init(PyInterpreterState_Main());
        PyGILState_Release(lock);
    }
    return thread;
}

bool Generator::LoadPythonScript(const std::string& python_script_path) {
    DARWIN_LOGGER;

    if(python_script_path.empty()){
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No python script to load");
        return true;
    }

    std::ifstream f(python_script_path.c_str());
    if(f.bad()) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : failed to open " + python_script_path);
        return false;
    }
    f.close();

    std::size_t pos = python_script_path.rfind("/");
    auto folders = python_script_path.substr(0, pos);
    auto filename = python_script_path.substr(pos+1, std::string::npos);
    if((pos = filename.rfind(".py")) != std::string::npos) {
        filename.erase(pos, std::string::npos);
    }

    Py_Initialize();
    PyEval_InitThreads();

    if(PyExceptionCheckAndLog("Generator::LoadPythonScript : Error during python init :")) {
        return false;
    }
    if((pName = PyUnicode_DecodeFSDefault(filename.c_str())) == nullptr){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : error importing string " + filename + " : ");
        return false;
    }

    if (PyRun_SimpleString("import sys") != 0) {
        DARWIN_LOG_DEBUG("Generator::LoadPythonScript : An error occurred while loading the 'sys' module");
        return false;
    }

    std::string command = "sys.path.append(\"" + folders + "\")";
    if (PyRun_SimpleString(command.c_str()) != 0) {
        DARWIN_LOG_DEBUG(
                "Generator::LoadPythonScript : An error occurred while appending the custom path '" +
                folders +
                "' to the Python path"
        );
        return false;
    }

    if (PyRun_SimpleString("sys.path.append('/tmp/pydebug/lib/python3.8/site-packages/')") != 0) {
        DARWIN_LOG_DEBUG(
                "Generator::LoadPythonScript : An error occurred while appending the custom path '" +
                folders +
                "' to the Python path"
        );
        return false;
    }

    if((pModule = PyImport_Import(*pName)) == nullptr) {
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python script module '" + filename + "' : ");
        return false;
    }

    functions.configPyFunc.f.py = PyObject_GetAttrString(*pModule, "filter_config");
    if(functions.configPyFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_config' method in the python script");
    } else if (! PyCallable_Check(functions.configPyFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : filter_config symbol exists but is not callable");
        return false;
    } else {
        functions.configPyFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.parseBodyFunc.f.py = PyObject_GetAttrString(*pModule, "parse_body");
    if(functions.parseBodyFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'parse_body' method in the python script");
    } else if (! PyCallable_Check(functions.parseBodyFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : parse_body symbol exists but is not callable");
        return false;
    } else {
        functions.parseBodyFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.preProcessingFunc.f.py = PyObject_GetAttrString(*pModule, "filter_pre_process");
    if(functions.preProcessingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_pre_process' method in the python script");
    } else if (! PyCallable_Check(functions.preProcessingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : filter_pre_process symbol exists but is not callable");
        return false;
    } else {
        functions.preProcessingFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.processingFunc.f.py = PyObject_GetAttrString(*pModule, "filter_process");
    if(functions.processingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_process' method in the python script");
    } else if (! PyCallable_Check(functions.processingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : filter_process symbol exists but is not callable");
        return false;
    } else {
        functions.processingFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.alertFormatingFunc.f.py = PyObject_GetAttrString(*pModule, "alert_formating");
    if(functions.alertFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'alert_formating' method in the python script");
    } else if (! PyCallable_Check(functions.alertFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : alert_formating symbol exists but is not callable");
        return false;
    } else {
        functions.alertFormatingFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.outputFormatingFunc.f.py = PyObject_GetAttrString(*pModule, "output_formating");
    if(functions.outputFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'output_formating' method in the python script");
    } else if (! PyCallable_Check(functions.outputFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : output_formating symbol exists but is not callable");
        return false;
    } else {
        functions.outputFormatingFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    functions.responseFormatingFunc.f.py = PyObject_GetAttrString(*pModule, "response_formating");
    if(functions.responseFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'response_formating' method in the python script");
    } else if (! PyCallable_Check(functions.responseFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : response_formating symbol exists but is not callable");
        return false;
    } else {
        functions.responseFormatingFunc.loc = FunctionOrigin::PYTHON_MODULE;
    }

    return true;
}

std::shared_ptr<darwin::ATask>
Generator::CreateTask(darwin::session_ptr_t s) noexcept {
    return std::static_pointer_cast<darwin::ATask>(
            std::make_shared<PythonTask>(_cache, _cache_mutex, s, s->_packet,
            *pModule, functions)
        );
}

long Generator::GetFilterCode() const {
    return DARWIN_FILTER_PYTHON;
}

Generator::~Generator() {
    PyGILState_Ensure();
    Py_Finalize();
    // Python environment is destroyed, we can't release the GIL
}
