/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/PythonUtils.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();

private:
    bool LoadConfigurationFile(const rapidjson::Document &configuration);
    bool LoadPythonCode(const std::string& python_env_path_str, const std::string& module_str,
                        const std::string& function_str, const std::string* custom_python_path_str=nullptr);

public:
    // The config file is the database here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    wchar_t *_program_name = nullptr; // the Python environment path to load
    PyObject *_py_module = nullptr; // the Python module to load in the environment
    PyObject *_py_function = nullptr; // the Python function to call in the module

    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};