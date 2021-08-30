/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/PythonUtils.hpp"
#include "Session.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"

template<typename F>
union FunctionUnion{
    PyObject* py;
    F so;

    FunctionUnion() {
        this->py = nullptr;
    }

    ~FunctionUnion(){}
};

enum FunctionOrigin {
    NONE,
    PYTHON_MODULE,
    SHARED_LIBRARY,
};

struct FunctionHolder{
    FunctionHolder(): parseBodyOrigin{FunctionOrigin::NONE}, preProcessingOrigin{FunctionOrigin::NONE},
        alertFormatingOrigin{FunctionOrigin::NONE}, outputFormatingOrigin{FunctionOrigin::NONE},
        responseFormatingOrigin{FunctionOrigin::NONE} { }
    ~FunctionHolder() = default;
    FunctionHolder(FunctionHolder const &) = delete;
    FunctionHolder(FunctionHolder &&) = delete;
    FunctionHolder& operator=(FunctionHolder const &) = delete;
    FunctionHolder& operator=(FunctionHolder &&) = delete;

    typedef PyObject*(*parse_body_t)(const std::string&);
    typedef PyObject*(*process_t)(PyObject*);
    typedef std::string(*format_t)(PyObject*);
    
    FunctionUnion<parse_body_t> parseBodyFunc;
    FunctionOrigin parseBodyOrigin;
    
    FunctionUnion<process_t> processingFunc;
    FunctionOrigin processingOrigin;

    FunctionUnion<process_t> preProcessingFunc;
    FunctionOrigin preProcessingOrigin;

    FunctionUnion<format_t> alertFormatingFunc;
    FunctionOrigin alertFormatingOrigin;

    FunctionUnion<format_t> outputFormatingFunc;
    FunctionOrigin outputFormatingOrigin;

    FunctionUnion<format_t> responseFormatingFunc;
    FunctionOrigin responseFormatingOrigin;

};

class Generator: public AGenerator {
public:
    Generator();
    ~Generator();

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

    bool LoadPythonScript(const std::string& python_script_path);
    bool LoadSharedLibrary(const std::string& shared_library_path);
    bool CheckConfig();
    
    PyObject* pName;
    PyObject* pModule;

    FunctionHolder functions;
    
};