/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/PythonUtils.hpp"
#include "ATask.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"
#include "fpython.hpp"
#include "PythonThread.hpp"

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

template<typename F>
struct FunctionPySo {
    FunctionOrigin loc;
    FunctionUnion<F> f;

    FunctionPySo(): loc{FunctionOrigin::NONE} { }
    ~FunctionPySo() = default;
};


struct FunctionHolder{
    FunctionHolder() = default;
    ~FunctionHolder() = default;

    FunctionHolder(FunctionHolder const &) = delete;
    FunctionHolder(FunctionHolder &&) = delete;
    FunctionHolder& operator=(FunctionHolder const &) = delete;
    FunctionHolder& operator=(FunctionHolder &&) = delete;

    typedef PyObject*(*parse_body_t)(PyObject*, const std::string&);
    typedef PyObject*(*process_t)(PyObject*, PyObject*);
    typedef std::list<std::string>(*alert_format_t)(PyObject*, PyObject*);
    typedef DarwinResponse(*resp_format_t)(PyObject*, PyObject*);
    typedef bool(*config_t)(rapidjson::Document const &);
    FunctionPySo<parse_body_t> parseBodyFunc;
    
    FunctionPySo<process_t> processingFunc;
    FunctionPySo<process_t> preProcessingFunc;

    FunctionPySo<alert_format_t> alertFormatingFunc;
    FunctionPySo<resp_format_t> outputFormatingFunc;
    FunctionPySo<resp_format_t> responseFormatingFunc;
    FunctionPySo<config_t> configPyFunc; 
    FunctionPySo<config_t> configSoFunc; 
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

    bool LoadPythonScript(const std::string& python_script_path);
    bool LoadSharedLibrary(const std::string& shared_library_path);
    bool CheckConfig() const;
    bool SendConfig(rapidjson::Document const& config) const;
    bool SendPythonConfig(rapidjson::Document const& config) const;

    PyObject* pName;
    PyObject* pModule;

    FunctionHolder functions;
    
};