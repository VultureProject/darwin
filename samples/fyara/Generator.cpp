/// \file     Generator.cpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "YaraTask.hpp"
#include "Generator.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Yara:: Generator:: Loading configuration...");
    _yaraCompiler = std::make_shared<darwin::toolkit::YaraCompiler>();

    if(not _yaraCompiler->Init()) {
        DARWIN_LOG_CRITICAL("Yara::Generator:: Could not initialize yara compiler");
        return false;
    }

    if (!configuration.IsObject()) {
        DARWIN_LOG_CRITICAL("Yara::Generator:: Configuration is not a JSON object");
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
        _fastmode = true;
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
            fclose(pfile);
        }
    }

    if (_yaraCompiler->GetStatus() == darwin::toolkit::YaraCompiler::Status::NEW_RULES) {
        if(not _yaraCompiler->CompileRules()) {
            DARWIN_LOG_CRITICAL("Yara::Generator:: could not compile rules");
            return false;
        }
    }
    else {
        DARWIN_LOG_ERROR("Yara::Generator:: no rules added");
        return false;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<YaraTask>(socket, manager, _cache, _cache_mutex, _yaraCompiler->GetEngine(_fastmode, _timeout)));
}
