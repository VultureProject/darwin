/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "LogsTask.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Logs :: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("Logs:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Logs:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("Logs:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("Logs:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("Logs:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Logs:: Generator:: Loading classifier...");

    std::string redis_socket_path;

    _redis = configuration.HasMember("redis_socket_path");
    _log = configuration.HasMember("log_file_path");

    if (!_redis and !_log){
        DARWIN_LOG_CRITICAL("Logs:: Generator:: Need at least one of these parameters: "
                            "\"redis_socket_path\" or \"log_file_path\"");
        return false;
    }

    if (_redis){
        if (!configuration["redis_socket_path"].IsString()) {
            DARWIN_LOG_CRITICAL("Logs:: Generator:: \"redis_socket_path\" needs to be a string");
            return false;
        }

        if (!configuration.HasMember("redis_list_name")){
            DARWIN_LOG_CRITICAL("Logs:: Generator:: Missing parameter : \"redis_list_name\"");
            return false;
        }

        if (!configuration["redis_list_name"].IsString()) {
            DARWIN_LOG_CRITICAL("Logs:: Generator:: \"redis_list_name\" needs to be a string");
            return false;
        }
        redis_socket_path = configuration["redis_socket_path"].GetString();
        _redis_list_name = configuration["redis_list_name"].GetString();

        if(!ConfigRedis(redis_socket_path)){
            DARWIN_LOG_CRITICAL("Logs:: Generator:: Error when configuring REDIS");
            return false;
        }
    }

    if (_log){
        if (!configuration["log_file_path"].IsString()) {
            DARWIN_LOG_CRITICAL("Logs:: Generator:: \"log_file_path\" needs to be a string");
            return false;
        }
        _log_file_path = configuration["log_file_path"].GetString();

        // Open file and check permissions (and create file if not existing)
        _log_file.open(_log_file_path, std::ios::out | std::ios::app);
        if(!_log_file.is_open() or _log_file.fail()) {
            DARWIN_LOG_ERROR("LogsGenerator::LoadClassifier:: Error when opening the log file, "
                         "maybe too low space disk or wrong permission");
            return false;
        }
    }

    return true;
}

bool Generator::ConfigRedis(std::string redis_socket_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Logs:: Generator:: Redis configuration...");

    _redis_manager = std::make_shared<darwin::toolkit::RedisManager>(redis_socket_path);

    /* Ignore signals for broken pipes.
     * Otherwise, if the Redis UNIX socket does not exist anymore,
     * this filter will crash */
    signal(SIGPIPE, SIG_IGN);

    if (!_redis_manager->ConnectToRedis(true)) return false;

    DARWIN_LOG_DEBUG("Logs:: ConfigRedis:: Redis configured");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<LogsTask>(socket, manager, _cache,
                                       _log, _redis,
                                       _log_file_path, _log_file,
                                       _redis_list_name, _redis_manager));
}

Generator::~Generator(){
    _log_file.close();
};
