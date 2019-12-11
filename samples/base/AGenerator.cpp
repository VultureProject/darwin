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

bool AGenerator::ReadConfig(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AGenerator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("AGenerator:: Parsing configuration from '" + configuration_file_path + "'...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

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

    conf_file_stream.close();
    return true;
}
