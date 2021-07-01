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
    DARWIN_LOG_DEBUG("End:: Generator:: Parsing configuration from '" + configuration_file_path + "'...");

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

    std::string redis_socket_path, redis_ip;
    unsigned int redis_port = 6379;

    if(configuration.HasMember("redis_socket_path")) {
        if (not configuration["redis_socket_path"].IsString()) {
            DARWIN_LOG_CRITICAL("End:: Generator:: 'redis_socket_path' needs to be a string");
            return false;
        } else {
            redis_socket_path = configuration["redis_socket_path"].GetString();
        }
    }

    if(configuration.HasMember("redis_ip")) {
        if (not configuration["redis_ip"].IsString()) {
            DARWIN_LOG_CRITICAL("End:: Generator:: 'redis_ip' needs to be a string");
            return false;
        } else {
            redis_ip = configuration["redis_ip"].GetString();
        }
    }

    if(configuration.HasMember("redis_port")) {
        if (not configuration["redis_port"].IsUint()) {
            DARWIN_LOG_CRITICAL("End:: Generator:: 'redis_port' needs to be an unsigned integer");
            return false;
        } else {
            redis_port = configuration["redis_port"].GetUint();
        }
    }

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    // Done in AlertManager before arriving here, but will allow better transition from redis singleton
    if (not redis_socket_path.empty()) {
        redis.SetUnixConnection(redis_socket_path);
    } else if (not redis_ip.empty()) {
        redis.SetIpConnection(redis_ip, redis_port);
    } else {
        DARWIN_LOG_ERROR("End:: Generator:: no valid way to connect to Redis, "
                            "please set 'redis_socket_path' or 'redis_ip' (and optionally 'redis_port').");
        return false;
    }
    return redis.FindAndConnect();
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<EndTask>(socket, manager, _cache));
}

Generator::~Generator() = default;
