/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "PythonExampleTask.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("PythonExample:: Generator:: Configuring...");

    DARWIN_LOG_DEBUG("PythonExample:: Generator:: Please insert your real configuration code here. "
                     "For now, we just import a dummy Python function");

    DARWIN_LOG_DEBUG("PythonExample:: Generator:: Parsing configuration from \"" + configFile + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configFile, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("PythonExample:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    if (!LoadConfigurationFile(configuration)) {
        DARWIN_LOG_ERROR("PythonExample:: Generator:: Something went wrong while parsing the configuration file");
        return false;
    }

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("PythonExample:: Generator:: Configured");
    return true;
}

bool Generator::LoadConfigurationFile(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("PythonExampleTask:: Generator:: Loading configuration file...");

    if (!configuration.HasMember("python_env_path")) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: Missing parameter: \"python_env_path\"");
        return false;
    }

    if (!configuration["python_env_path"].IsString()) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: \"python_env_path\" needs to be a string");
        return false;
    }

    std::string python_env_path_str = configuration["python_env_path"].GetString();

    if (!configuration.HasMember("module")) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: Missing parameter: \"module\"");
        return false;
    }

    if (!configuration["module"].IsString()) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: \"module\" needs to be a string");
        return false;
    }

    std::string module_str = configuration["module"].GetString();

    if (!configuration.HasMember("function")) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: Missing parameter: \"function\"");
        return false;
    }

    if (!configuration["function"].IsString()) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: \"function\" needs to be a string");
        return false;
    }

    std::string function_str = configuration["function"].GetString();

    bool is_custom_path = configuration.HasMember("custom_python_path");

    if (!is_custom_path) {
        return LoadPythonCode(python_env_path_str, module_str, function_str);
    }

    if (!configuration["custom_python_path"].IsString()) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: Generator:: \"custom_python_path\" needs to be a string");
        return false;
    }

    std::string custom_python_path_str = configuration["custom_python_path"].GetString();

    return LoadPythonCode(python_env_path_str, module_str, function_str, &custom_python_path_str);
}

bool Generator::LoadPythonCode(const std::string& python_env_path_str, const std::string& module_str,
                               const std::string& function_str, const std::string* custom_python_path_str) {
    DARWIN_LOGGER;

    if (!darwin::pythonutils::InitPythonProgram(python_env_path_str, &_program_name, custom_python_path_str)) {
        DARWIN_LOG_CRITICAL("PythonExample:: Generator:: Could not initialize generator: "
                            "Python program initialization went wrong");
        return false;
    }

    if (!darwin::pythonutils::ImportPythonModule(module_str, &_py_module)) {
        DARWIN_LOG_CRITICAL("PythonExample:: Generator:: Could not initialize generator: "
                            "Python module import went wrong");
        return false;
    }

    if (!darwin::pythonutils::GetPythonFunction(_py_module, function_str, &_py_function)) {
        DARWIN_LOG_CRITICAL("PythonExample:: Generator:: Could not initialize generator: "
                            "Something went wrong while getting Python function");
        return false;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<PythonExampleTask>(socket, manager, _cache, _py_function));
}

Generator::~Generator() {
    darwin::pythonutils::ExitPythonProgram(&_program_name);
}