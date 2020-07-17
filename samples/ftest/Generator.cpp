/// \file     Generator.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     11/12/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "TestTask.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "AlertManager.hpp"

bool Generator::ConfigureAlerting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Test:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("Test:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Test:: Generator:: Loading Configuration...");
    std::string redis_socket_path;

    if (configuration.HasMember("fail_config"))
        return false;
    if(configuration.HasMember("redis_socket_path")) {
        if (configuration["redis_socket_path"].IsString()) {
            redis_socket_path = configuration["redis_socket_path"].GetString();

            if (configuration.HasMember("redis_list_name")){
                if (configuration["redis_list_name"].IsString()) {
                    _redis_list_name = configuration["redis_list_name"].GetString();
                    DARWIN_LOG_INFO("Test:: Generator:: 'redis_list_name' set to " + _redis_list_name);
                }
            }

            if (configuration.HasMember("redis_channel_name")){
                if (configuration["redis_channel_name"].IsString()) {
                    _redis_channel_name = configuration["redis_channel_name"].GetString();
                    DARWIN_LOG_INFO("Test:: Generator:: 'redis_channel_name' set to " + _redis_channel_name);
                }
            }

            if(not ConfigRedis(redis_socket_path)){
                DARWIN_LOG_WARNING("Test:: Generator:: Error when configuring REDIS");
                return true;
            }
        }
        else {
            DARWIN_LOG_WARNING("Test:: Generator:: 'redis_socket_path' needs to be a string, ignoring");
        }
    }

    return true;
}

bool Generator::ConfigRedis(std::string redis_socket_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Test:: Generator:: Redis configuration...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    // Done in AlertManager before arriving here, but will allow better transition from redis singleton
    redis.SetUnixConnection(redis_socket_path);
    return redis.FindAndConnect();
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<TestTask>(socket, manager, _cache, _cache_mutex, _redis_list_name, _redis_channel_name));
}
