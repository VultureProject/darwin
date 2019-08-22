/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "base/Logger.hpp"
#include "Generator.hpp"
#include "EndTask.hpp"

#include <fstream>
#include <string>

#include "../../toolkit/RedisManager.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("End:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("End:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("End:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("End:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("End:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("End:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("End:: Generator:: Loading classifier...");

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("End:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("End:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    _redis_socket_path = configuration["redis_socket_path"].GetString();

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<EndTask>(socket, manager, _cache, _redis_socket_path));
}

Generator::~Generator() = default;
