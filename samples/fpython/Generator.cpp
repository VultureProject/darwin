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
#include "AlertManager.hpp"
#include "fpython.hpp"

Generator::Generator(): pName{nullptr}, pModule{nullptr}
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

    return LoadPythonScript(python_script_path) && LoadSharedLibrary(shared_library_path) && CheckConfig();
}

bool Generator::LoadSharedLibrary(const std::string& shared_library_path) {
    DARWIN_LOGGER;

    if(shared_library_path.empty()){
        DARWIN_LOG_INFO("Generator::LoadSharedLibrary : No shared library to load");
        return true;
    }

    void* handle = dlopen(shared_library_path.c_str(), RTLD_LAZY);
    if(handle == nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadSharedLibrary : Error loading the shared library : failed to open " + shared_library_path);
        return false;
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

bool Generator::CheckConfig() {
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
    if(PyErr_Occurred() != nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error post py init");
        PyErr_Print();
    }
    pName = PyUnicode_DecodeFSDefault(filename.c_str());
    // PyObject* pFolders = PyUnicode_DecodeFSDefault(folders.c_str());
    // PyObject* pFilename = PyUnicode_DecodeFSDefault(filename.c_str());
    if(PyErr_Occurred() != nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error post filename decode");
        PyErr_Print();
    }
    if (PyRun_SimpleString("import sys") != 0) {
        DARWIN_LOG_DEBUG("darwin:: pythonutils:: InitPythonProgram:: An error occurred while loading the 'sys' module");
        if(PyErr_Occurred() != nullptr) PyErr_Print();

        return false;
    }
    if(PyErr_Occurred() != nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error post imp sys");
        PyErr_Print();
    }

    std::string command = "sys.path.append(\"" + folders + "\")";
    if (PyRun_SimpleString(command.c_str()) != 0) {
        DARWIN_LOG_DEBUG(
                "darwin:: pythonutils:: InitPythonProgram:: An error occurred while appending the custom path '" +
                folders +
                "' to the Python path"
        );

        if(PyErr_Occurred() != nullptr) PyErr_Print();

        return false;
    }

    if(PyErr_Occurred() != nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error post sys path");
        PyErr_Print();
    }

    pModule = PyImport_Import(pName);

    if(PyErr_Occurred() != nullptr) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : post import");

        PyErr_Print();
    }

    if(pModule == nullptr) {        
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error while attempting to load the python script module");
        if(PyErr_Occurred() != nullptr) PyErr_Print();

        return false;
    }
    
    functions.parseBodyFunc.f.py = PyObject_GetAttrString(pModule, "parse_body");
    if(functions.parseBodyFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'parse_body' method in the python script");
    } else if (! PyCallable_Check(functions.parseBodyFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : parse_body symbol exists but is not callable");
        return false;
    } else {
        functions.parseBodyFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    functions.preProcessingFunc.f.py = PyObject_GetAttrString(pModule, "filter_pre_process");
    if(functions.preProcessingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_pre_process' method in the python script");
    } else if (! PyCallable_Check(functions.preProcessingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : filter_pre_process symbol exists but is not callable");
        return false;
    } else {
        functions.preProcessingFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    functions.processingFunc.f.py = PyObject_GetAttrString(pModule, "filter_process");
    if(functions.processingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'filter_process' method in the python script");
    } else if (! PyCallable_Check(functions.processingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : filter_process symbol exists but is not callable");
        return false;
    } else {
        functions.processingFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    functions.alertFormatingFunc.f.py = PyObject_GetAttrString(pModule, "alert_formating");
    if(functions.alertFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'alert_formating' method in the python script");
    } else if (! PyCallable_Check(functions.alertFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : alert_formating symbol exists but is not callable");
        return false;
    } else {
        functions.alertFormatingFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    functions.outputFormatingFunc.f.py = PyObject_GetAttrString(pModule, "output_formating");
    if(functions.outputFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'output_formating' method in the python script");
    } else if (! PyCallable_Check(functions.outputFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : output_formating symbol exists but is not callable");
        return false;
    } else {
        functions.outputFormatingFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    functions.responseFormatingFunc.f.py = PyObject_GetAttrString(pModule, "response_formating");
    if(functions.responseFormatingFunc.f.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No 'response_formating' method in the python script");
    } else if (! PyCallable_Check(functions.responseFormatingFunc.f.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : response_formating symbol exists but is not callable");
        return false;
    } else {
        functions.responseFormatingFunc.loc= FunctionOrigin::PYTHON_MODULE;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<PythonTask>(socket, manager, _cache, _cache_mutex, 
                pModule, functions));
}


Generator::~Generator() {
    Py_Finalize();
} 