/// \file     Generator.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "base/Logger.hpp"
#include "Generator.hpp"
#include "TAnomalyTask.hpp"
#include "TAnomalyThreadManager.hpp"

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("TAnomaly:: Generator:: Loading configuration...");

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    std::string redis_socket_path = configuration["redis_socket_path"].GetString();
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    if(not redis.SetUnixPath(redis_socket_path)) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Could not connect to Redis socket '" + redis_socket_path + "'.");
        return false;
    }

    if (configuration.HasMember("redis_list_name")) {

        if (!configuration["redis_list_name"].IsString()) {
            DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: \"redis_list_name\" needs to be a string");
            return false;
        }

        _redis_list_name = configuration["redis_list_name"].GetString();
    }

    if (!configuration.HasMember("log_file_path")) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Missing parameter: \"log_file_path\"");
        return false;
    }

    if (!configuration["log_file_path"].IsString()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: \"log_file_path\" needs to be a string");
        return false;
    }

    _log_file_path = configuration["log_file_path"].GetString();

    _anomaly_thread_manager = std::make_shared<AnomalyThreadManager>(_log_file_path, _redis_list_name);
    if(!_anomaly_thread_manager->Start()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Error when starting polling thread");
        return false;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<AnomalyTask>(socket, manager, _cache, _cache_mutex,
                                          _anomaly_thread_manager, _redis_list_name));
}