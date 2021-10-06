/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "Logger.hpp"
#include "../../toolkit/PythonUtils.hpp"
#include "ATask.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"
#include "fpython.hpp"
#include "PythonThread.hpp"
#include "PythonObject.hpp"

///
/// \brief Unions representing either a function pointer from a shared object or a python function reference
/// 
/// \tparam F the prototype of the function (if it comes from a shared object)
///
template<typename F>
union FunctionUnion{
    PyObject* py;
    F so;

    FunctionUnion() {
        this->py = nullptr;
    }

    ~FunctionUnion(){}
};

enum class FunctionOrigin {
    NONE,
    PYTHON_MODULE,
    SHARED_LIBRARY,
};

///
/// \brief tagged union containing a FunctionUnion and its tag FunctionOrigin
/// 
/// \tparam F prototype of the function if loaded from a shared object
///
template<typename F>
struct FunctionPySo {
    FunctionOrigin loc;
    FunctionUnion<F> f;

    FunctionPySo(): loc{FunctionOrigin::NONE} { }
    ~FunctionPySo() = default;
};

///
/// \brief Struct holding the function pointers for the python filter
///        It should be unique and passed by reference
/// 
///
struct FunctionHolder{
    FunctionHolder() = default;
    ~FunctionHolder() = default;

    FunctionHolder(FunctionHolder const &) = delete;
    FunctionHolder(FunctionHolder &&) = delete;
    FunctionHolder& operator=(FunctionHolder const &) = delete;
    FunctionHolder& operator=(FunctionHolder &&) = delete;

    typedef bool(*config_t)(rapidjson::Document const &);
    typedef PyObject*(*parse_body_t)(PyObject*, const std::string&);
    typedef PyObject*(*process_t)(PyObject*, PyObject*);
    typedef std::vector<std::string>(*alert_format_t)(PyObject*, PyObject*);
    typedef DarwinResponse(*resp_format_t)(PyObject*, PyObject*);
    
    // There are 2 config functions as we accept to pass informations to both the python module and the shared library
    // Note that the shared library has a direct access to the python module
    FunctionPySo<config_t> configPyFunc; 
    FunctionPySo<config_t> configSoFunc; 
    
    FunctionPySo<parse_body_t> parseBodyFunc;
    
    FunctionPySo<process_t> processingFunc;
    FunctionPySo<process_t> preProcessingFunc;

    FunctionPySo<process_t> contextualizeFunc;
    FunctionPySo<process_t> alertContextualizeFunc;

    FunctionPySo<alert_format_t> alertFormatingFunc;
    FunctionPySo<resp_format_t> outputFormatingFunc;
    FunctionPySo<resp_format_t> responseFormatingFunc;
};

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator();

    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const;
    static PythonThread& GetPythonThread();
    static bool PyExceptionCheckAndLog(std::string const& prefix_log, darwin::logger::log_type log_type=darwin::logger::Critical);

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

    ///
    /// \brief loads the python script which path is given and attempts to load all defined functions
    ///        If the path is not empty, a python interpreter will be initialized (by a call to Py_Initiliaze())
    /// 
    /// \param python_script_path 
    /// \return true if the script was correctly loaded or if the path is empty
    /// \return false if the path can't be read, if loading the module triggered an error 
    ///               or if a symbol with the name of a defined function is found but not callable
    ///
    bool LoadPythonScript(const std::string& python_script_path);

    ///
    /// \brief loads the shared library which path is given and attempts to load all defined functions
    /// 
    /// \param shared_library_path 
    /// \return true if the SO was correctly loaded or if the path is empty
    /// \return false if the shared library can't be read or if loading the SO triggered an error
    ///
    bool LoadSharedLibrary(const std::string& shared_library_path);

    ///
    /// \brief Verifies that the configuration if valid
    ///        filter_preprocess, filter_process, alert_formating, 
    ///        response_formating and output_formating must be set
    ///        parse_body and filter_config are optional
    /// 
    /// \return true the configuration is valid
    ///
    bool CheckConfig() const;

    ///
    /// \brief Dispatches the configuration to the Shared object (if any) and the python module (if any)
    /// 
    /// \param config parsed configuration
    /// \return true the module and/or the SO sent back true
    ///
    bool SendConfig(rapidjson::Document const& config);

    ///
    /// \brief Sends the configuration to the Python module
    /// 
    /// \param config 
    /// \return false an error occured whil calling the python function or it sent back false
    ///
    bool SendPythonConfig(rapidjson::Document const& config);

    PyObjectOwner pModule;

    FunctionHolder functions;
    
};