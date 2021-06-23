/// \file     AGenerator.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     16/10/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string>
#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "AGenerator.hpp"
#include "AlertManager.hpp"

AGenerator::AGenerator(size_t nb_task_threads): _threadPool{GetThreadPoolOptions(nb_task_threads)}
{
    ;
}

tp::ThreadPoolOptions AGenerator::GetThreadPoolOptions(size_t nb_task_threads){
    auto opts = tp::ThreadPoolOptions();
    opts.setThreadCount(nb_task_threads);
    return opts;
}

bool AGenerator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AGenerator:: Configuring...");

    if (!this->ReadConfig(configFile)) return false;

    DARWIN_LOG_DEBUG("AGenerator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("AGenerator:: Configured");
    return true;
}

bool AGenerator::ConfigureNetworkObject(boost::asio::io_context &context __attribute__((unused))) {
    return true;
}

bool AGenerator::ExtractCustomAlertingTags(const rapidjson::Document &configuration,
                                           std::string& tags) {
    DARWIN_LOGGER;

    if (not configuration.HasMember("alert_tags")) {
        DARWIN_LOG_DEBUG("AGenerator:: No alert_tags found in configuration");
        return false;
    }
    if (not configuration["alert_tags"].IsArray()) {
        DARWIN_LOG_WARNING("AGenerator:: Given alert_tags is not an array. Using defaults.");
        return false;
    }
    auto array = configuration["alert_tags"].GetArray();
    tags = "[";
    for (auto& tag : array) {
        if (not tag.IsString()) {
            DARWIN_LOG_WARNING("AGenerator:: One alert tag is not a string. Ignoring.");
            continue;
        }
        if (tag.GetStringLength() < 1) {
            DARWIN_LOG_WARNING("AGenerator:: One alert tag is an empty string. Ignoring.");
            continue;
        }
        if (tags.length() > 1)
            tags += ',';
        tags += '"';
        tags += tag.GetString();
        tags += '"';
    }
    tags += "]";
    return true;
}

bool AGenerator::ReadConfig(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AGenerator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("AGenerator:: Parsing configuration from '" + configuration_file_path + "'...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);
    std::string alerting_tags;

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("AGenerator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("AGenerator:: Reading configuration...");

    if (!configuration.IsObject()) {
        DARWIN_LOG_CRITICAL("AGenerator:: Configuration is not a JSON object");
        conf_file_stream.close();
        return false;
    }

    if (not darwin::AlertManager::instance().Configure(configuration)) {
        DARWIN_LOG_WARNING("AGenerator:: An error occured configuring alerting. "
                           "Partial or no configuration was applied. "
                           "Continuing..."
        );
    }

    if (!this->LoadConfig(configuration)) {
        conf_file_stream.close();
        return false;
    }

    // Extract custom alerting tags
    if (this->ExtractCustomAlertingTags(configuration, alerting_tags)) {
        if (not this->ConfigureAlerting(alerting_tags)) return false;
    } else {
        this->ConfigureAlerting("");
    }

    conf_file_stream.close();
    return true;
}

tp::ThreadPool& AGenerator::GetTaskThreadPool() {
    return this->_threadPool;
}
