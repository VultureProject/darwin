/// \file     Generator.cpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "YaraScanTask.hpp"
#include "Generator.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Yara:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("Yara:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Yara:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("Yara:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("Yara:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("Yara:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Yara:: Generator:: Loading classifier...");
    _yaraCompiler = std::make_shared<darwin::toolkit::YaraCompiler>();

    if (!configuration.IsObject()) {
        DARWIN_LOG_CRITICAL("Yara:: Generator:: Configuration is not a JSON object");
        return false;
    }

    if(configuration.HasMember("fastmode")) {
        if(!configuration["fastmode"].IsBool()) {
            DARWIN_LOG_CRITICAL("Yara::Generator:: \"fastmode\" needs to be a boolean");
            return false;
        }
        _fastmode = configuration["fastmode"].GetBool();
    }
    else {
        _fastmode = false;
    }

    if(configuration.HasMember("timeout")) {
        if(!configuration["timeout"].IsUint()) {
            DARWIN_LOG_CRITICAL("Yara::Generator:: \"timeout\" needs to be a positive integer");
            return false;
        }
        _timeout = configuration["timeout"].GetUint();
    }
    else {
        _timeout = 0;
    }

    if(!configuration.HasMember("rule_file_list")) {
        DARWIN_LOG_CRITICAL("Yara::Generator:: \"rule_file_list\" parameter missing");
        return false;
    }

    if(!configuration["rule_file_list"].IsArray()) {
        DARWIN_LOG_CRITICAL("Yara::Generator:: \"rule_file_list\" needs to be a list, even if you provide only a single file");
        return false;
    }

    auto yara_rule_files = configuration["rule_file_list"].GetArray();

    if(yara_rule_files.Size() <= 0) {
        DARWIN_LOG_CRITICAL("Yara::Generator:: \"rule_file_list\" is empty, you should at least provide one valid yara rule file");
        return false;
    }

    for(auto& rule_file : yara_rule_files) {
        if(!rule_file.IsString()) {
            DARWIN_LOG_CRITICAL("Yara::Generator:: the entries in \"rule_file_list\" should be strings");
            return false;
        }

        std::string filename = rule_file.GetString();
        FILE *pfile = fopen(filename.c_str(), "r");

        if(!pfile) {
            DARWIN_LOG_ERROR("Yara::Generator:: could not open file '" + filename + "'");
        }
        else {
            _yaraCompiler->AddRuleFile(pfile, "", filename);
        }
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<YaraScanTask>(socket, manager, _cache, _yaraCompiler->GetEngine(_fastmode, _timeout)));
}

Generator::~Generator() {
}