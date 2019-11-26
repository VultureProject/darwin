/// \file     Generator.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     25/11/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/PythonUtils.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"
#include "Session.hpp"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator();

private:
    bool LoadPythonCode(const std::string& python_env_path_str, const std::string& module_str,
                        const std::string& function_str, const std::string* custom_python_path_str=nullptr);

public:
    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

protected:
    virtual bool LoadConfig(const rapidjson::Document& configuration) override final;

private:
    wchar_t *_program_name = nullptr; // the Python environment path to load
    PyObject *_py_module = nullptr; // the Python module to load in the environment
    PyObject *_py_function = nullptr; // the Python function to call in the module
};