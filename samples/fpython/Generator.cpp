/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <fstream>
#include <filesystem>
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

    // Early return if we don't have to log
    if (log_type < log.getLevel()) {
        return true;
    }

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

static PyObject* darwin_log(PyObject *self __attribute__((__unused__)), PyObject* args) {
    DARWIN_LOGGER;
    int tag = 0;
    const char* msg = nullptr;
    ssize_t msgLen = 0;
    if(PyArg_ParseTuple(args, "is#", &tag, &msg, &msgLen) != 1){
        Generator::PyExceptionCheckAndLog("darwin_log: Error parsing arguments :");
        Py_RETURN_NONE;
    }

    log.log(static_cast<darwin::logger::log_type>(tag), std::string(msg, msgLen));

    Py_RETURN_NONE;
}

static PyMethodDef logMethod[] = {
    {"log",  darwin_log, METH_VARARGS, "sends log to darwin"},
    {nullptr, nullptr, 0, nullptr}        /* Sentinel */
};

static PyModuleDef darwin_log_mod = {
    PyModuleDef_HEAD_INIT,
    "darwin_logger",
    "Python interface for the fdarwin C++ logger",
    -1,
    logMethod,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

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

    std::string python_script_path, shared_library_path, python_venv_folder;

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

    if(configuration.HasMember("python_venv_folder")){
        if (!configuration["python_venv_folder"].IsString()) {
            DARWIN_LOG_CRITICAL("Python:: Generator:: 'python_venv_folder' needs to be a string");
            return false;
        } else {
            python_venv_folder = configuration["python_venv_folder"].GetString();
        }
    }

    DARWIN_LOG_DEBUG("Python:: Generator:: Loading configuration...");

    bool pythonLoad = LoadPythonScript(python_script_path, python_venv_folder);

    if(Py_IsInitialized() != 0) {
        // Release the GIL, This is safe to call because we just initialized the python interpreter
        PyEval_SaveThread();
    }

    return pythonLoad && LoadSharedLibrary(shared_library_path) && CheckConfig() && SendConfig(configuration);
}

bool Generator::SendConfig(rapidjson::Document const& config) {
    DARWIN_LOGGER;
    bool pyConfigRes = false, soConfigRes = false;
    switch(_functions.configPyFunc.origin) {
        case FunctionOrigin::none:
            DARWIN_LOG_INFO("Python:: Generator:: SendConfig: No configuration function found in python script");
            pyConfigRes = true;
            break;
        case FunctionOrigin::python_module:
            pyConfigRes = this->SendPythonConfig(config);
            break;
        case FunctionOrigin::shared_library:
            DARWIN_LOG_CRITICAL("Python:: Generator:: SendConfig: Incoherent configuration");
            return false;
    }

    switch(_functions.configSoFunc.origin) {
        case FunctionOrigin::none:
            DARWIN_LOG_INFO("Python:: Generator:: SendConfig: No configuration function found in shared object");
            soConfigRes = true;
            break;
        case FunctionOrigin::python_module:
            DARWIN_LOG_CRITICAL("Python:: Generator:: SendConfig: Incoherent configuration");
            return false;
        case FunctionOrigin::shared_library:
            try{
                soConfigRes = _functions.configSoFunc.so(config);
            } catch(std::exception const& e){
                DARWIN_LOG_CRITICAL("Python:: Generator:: SendConfig: error while sending config :" + std::string(e.what()));
                soConfigRes = false;
            }
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
    PyObject* globalDictionary = PyModule_GetDict(*_pModule);

    PyObjectOwner pConfig = PyRun_StringFlags(cmd.c_str(), Py_eval_input, globalDictionary, globalDictionary, nullptr);
    if(Generator::PyExceptionCheckAndLog("Python:: Generator:: SendPythonConfig: Converting config to Py Object :")){
        return false;
    }
    PyObjectOwner res;
    if ((res  = PyObject_CallFunctionObjArgs(_functions.configPyFunc.py, *pConfig, nullptr)) == nullptr) {
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

    LoadFunctionFromSO(handle, _functions.configSoFunc,           "filter_config");
    LoadFunctionFromSO(handle, _functions.parseBodyFunc,          "parse_body");
    LoadFunctionFromSO(handle, _functions.preProcessingFunc,      "filter_pre_process");
    LoadFunctionFromSO(handle, _functions.processingFunc,         "filter_process");
    LoadFunctionFromSO(handle, _functions.contextualizeFunc,      "filter_contextualize");
    LoadFunctionFromSO(handle, _functions.alertContextualizeFunc, "alert_contextualize");
    LoadFunctionFromSO(handle, _functions.alertFormatingFunc,     "alert_formating");
    LoadFunctionFromSO(handle, _functions.outputFormatingFunc,    "output_formating");
    LoadFunctionFromSO(handle, _functions.responseFormatingFunc,  "response_formating");

    return true;
}
template<typename F>
inline void Generator::LoadFunctionFromSO(void * lib_handle, FunctionPySo<F> &function_holder, const std::string& function_name) {
    DARWIN_LOGGER;
    F func = (F)dlsym(lib_handle, function_name.c_str());
    if(func == nullptr){
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No '" + function_name + "' function in the shared library");
    } else {
        function_holder.origin = FunctionOrigin::shared_library;
        function_holder.so = func;
    }
}

bool Generator::CheckConfig() const {
    if(_functions.preProcessingFunc.origin == FunctionOrigin::none
        || _functions.processingFunc.origin == FunctionOrigin::none
        || _functions.contextualizeFunc.origin == FunctionOrigin::none
        || _functions.alertContextualizeFunc.origin == FunctionOrigin::none
        || _functions.alertFormatingFunc.origin == FunctionOrigin::none
        || _functions.outputFormatingFunc.origin == FunctionOrigin::none
        || _functions.responseFormatingFunc.origin == FunctionOrigin::none) 
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

bool Generator::LoadPythonScript(const std::string& python_script_path, const std::string& python_venv_folder) {
    DARWIN_LOGGER;

    if(python_script_path.empty()){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : No python script to load");
        return false;
    }

    std::ifstream f(python_script_path.c_str());
    if(! f.is_open()) {
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
    PyObjectOwner pName;
    if((pName = PyUnicode_DecodeFSDefault(filename.c_str())) == nullptr){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : error importing string " + filename + " : ");
        return false;
    }
    
    if(PyImport_AddModule("darlog") == nullptr){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to import the logger module : ");
        return false;
    }

    PyObjectOwner logmod;
    if((logmod = PyModule_Create(&darwin_log_mod))== nullptr){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the logger module : ");
        return false;
    }

    PyObject* sys_modules = PyImport_GetModuleDict();
    if (PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while calling PyImport_GetModuleDict(): ")){
        return false;
    }
    PyDict_SetItemString(sys_modules, "darwin_logger", *logmod);
    if (PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while calling PyDict_SetItemString(): ")){
        return false;
    }

    if (PyRun_SimpleString("import sys") != 0) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : An error occurred while loading the 'sys' module");
        return false;
    }

    std::string command = "sys.path.append(\"" + folders + "\")";
    if (PyRun_SimpleString(command.c_str()) != 0) {
        DARWIN_LOG_CRITICAL(
                "Generator::LoadPythonScript : An error occurred while appending the custom path '" +
                folders +
                "' to the Python path"
        );
        return false;
    }

    if(!python_venv_folder.empty()){
        std::filesystem::path venv_path(python_venv_folder);
        venv_path /= std::filesystem::path("lib") / (std::string("python") + PYTHON_VERSION) / "site-packages";
        std::error_code _ec;
        if(!std::filesystem::exists(venv_path, _ec)) {
            DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Virtual Env does not exist : " + venv_path.string());
            return false;
        }
        command = "sys.path.insert(0, \"" + venv_path.string() + "\")";
        // PyRun_SimpleString("sys.path.insert(0, \"/home/myadvens.lan/tcartegnie/workspace/darwin/.venv/lib/python3.8/site-packages\")");
        if (PyRun_SimpleString(command.c_str()) != 0) {
            DARWIN_LOG_CRITICAL(
                    "Generator::LoadPythonScript : An error occurred while inserting the custom venv '" +
                    venv_path.string() +
                    "' to the Python path"
            );
            return false;
        }
    }

    if((_pModule = PyImport_Import(*pName)) == nullptr) {
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python script module '" + filename + "' : ");
        return false;
    }
    pName.Decref();

    _pClass = PyObject_GetAttrString(*_pModule, "Execution");
    if (PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python class 'Execution' : ")){
        return false;
    }
    if(*_pClass == nullptr || ! PyType_Check(*_pClass)) {
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error while attempting to load the python class 'Execution' : Not a Class");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.configPyFunc, "filter_config")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'filter_config' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.parseBodyFunc, "parse_body")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'parse_body' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.preProcessingFunc, "filter_pre_process")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'filter_pre_process' : ");
        return false;
    }
    if( ! LoadFunctionFromPython(*_pClass, _functions.processingFunc, "filter_process")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'filter_process' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.contextualizeFunc, "filter_contextualize")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'filter_contextualize' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.alertContextualizeFunc, "alert_contextualize")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'alert_contextualize' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.alertFormatingFunc, "alert_formating")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'alert_formating' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.outputFormatingFunc, "output_formating")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'output_formating' : ");
        return false;
    }

    if( ! LoadFunctionFromPython(*_pClass, _functions.responseFormatingFunc, "response_formating")){
        PyExceptionCheckAndLog("Generator::LoadPythonScript : Error while attempting to load the python function 'response_formating' : ");
        return false;
    }
    
    return true;
}

template<typename F>
inline bool Generator::LoadFunctionFromPython(PyObject* _pClass, FunctionPySo<F>& function_holder, const std::string& function_name){
    DARWIN_LOGGER;
    function_holder.py = PyObject_GetAttrString(_pClass, function_name.c_str());
    if(function_holder.py == nullptr) {
        DARWIN_LOG_INFO("Generator::LoadPythonScript : No '" + function_name + "' method in the python script");
    } else if (! PyCallable_Check(function_holder.py)){
        DARWIN_LOG_CRITICAL("Generator::LoadPythonScript : Error loading the python script : '" + function_name + "' symbol exists but is not callable");
        return false;
    } else {
        function_holder.origin = FunctionOrigin::python_module;
    }
    return true;
}

std::shared_ptr<darwin::ATask>
Generator::CreateTask(darwin::session_ptr_t s) noexcept {
    return std::static_pointer_cast<darwin::ATask>(
            std::make_shared<PythonTask>(_cache, _cache_mutex, s, s->_packet,
            *_pClass, _functions)
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
