/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <chrono>
#include <locale>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "SessionTask.hpp"

// The config file is the Redis UNIX Socket here
bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Session:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    /* Parse config file to fill-in attribute conf of this */
    DARWIN_LOG_DEBUG("Session:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("Session:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("Session:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("Session:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Loading classifier...");

    std::string redis_socket_path;

    if (!configuration.IsObject()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Configuration is not a JSON object");
        return false;
    }

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    redis_socket_path = configuration["redis_socket_path"].GetString();
    
    return ConfigRedis(redis_socket_path);
}

bool Generator::ConfigRedis(std::string redis_socket_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Redis configuration...");

    _redis_manager = std::make_shared<darwin::toolkit::RedisManager>(redis_socket_path);

    /* Ignore signals for broken pipes.
     * Otherwise, if the Redis UNIX socket does not exist anymore,
     * this filter will crash */
    signal(SIGPIPE, SIG_IGN);

    if (!_redis_manager->ConnectToRedis(true)) return false;

    DARWIN_LOG_DEBUG("Session:: ConfigRedis:: Redis configured");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<SessionTask>(socket, manager, _cache, _redis_manager));
}
