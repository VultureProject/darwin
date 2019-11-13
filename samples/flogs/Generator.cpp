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

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
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

        redis_socket_path = configuration["redis_socket_path"].GetString();

        if (configuration.HasMember("redis_list_name")){
            if (not configuration["redis_list_name"].IsString()) {
                DARWIN_LOG_CRITICAL("Logs:: Generator:: \"redis_list_name\" needs to be a string");
                return false;
            }
            _redis_list_name = configuration["redis_list_name"].GetString();
            DARWIN_LOG_INFO("Logs:: Generator:: \"redis_list_name\" set to " + _redis_list_name);
        }

        if (configuration.HasMember("redis_channel_name")){
            if (not configuration["redis_channel_name"].IsString()) {
                DARWIN_LOG_CRITICAL("Logs:: Generator:: \"redis_channel_name\" needs to be a string");
                return false;
            }
            _redis_channel_name = configuration["redis_channel_name"].GetString();
            DARWIN_LOG_INFO("Logs:: Generator:: \"redis_channel_name\" set to " + _redis_channel_name);
        }

        if(_redis_list_name.empty() and _redis_channel_name.empty()) {
            DARWIN_LOG_CRITICAL("Logs:: Generator:: if \"redis_socket_path\" is provided, you need to provide at least"
                                    " \"redis_list_name\" or \"redis_channel_name\"");
            return false;
        }

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
        _log_file = std::make_shared<darwin::toolkit::FileManager>(_log_file_path, true);
        if(!_log_file->Open()){
            DARWIN_LOG_CRITICAL("Logs:: Generator:: Error when opening log file");
            return false;
        }
    }

    return true;
}

bool Generator::ConfigRedis(std::string redis_socket_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Logs:: Generator:: Redis configuration...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    bool ret = redis.SetUnixPath(redis_socket_path);

    return ret;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<LogsTask>(socket, manager, _cache, _cache_mutex,
                                       _log, _redis,
                                       _log_file_path, _log_file,
                                       _redis_list_name, _redis_channel_name));
}
