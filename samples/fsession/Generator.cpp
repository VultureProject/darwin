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
#include "AlertManager.hpp"

bool Generator::ConfigureAlerting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Session:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("Session:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Loading configuration...");

    std::string redis_socket_path;

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Missing parameter: 'redis_socket_path'");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: 'redis_socket_path' needs to be a string");
        return false;
    }

    if (!configuration.HasMember("applications")) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Missing parameter: 'applications'");
        return false;
    }
    if (!configuration["applications"].IsArray()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' needs to be an array");
        return false;
    }

    int cpt=0;
    for (auto &app_tmp : configuration["applications"].GetArray()) {
        if (!app_tmp.IsArray()) {
            DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' sub-elements must be array");
            return false;
        }
        if (!app_tmp[0].IsString()) {
            DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' sub-elements first element must be a string");
            return false;
        }
        if (!app_tmp[1].IsString()) {
            DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' sub-elements second element must be a string");
            return false;
        }
        if (!app_tmp[2].IsString()) {
            DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' sub-elements third element must be a string");
            return false;
        }
        if (!app_tmp[3].IsUint64()) {
            DARWIN_LOG_CRITICAL("Session:: Generator:: 'applications' sub-elements fourth element must be an integer");
            return false;
        }
        // applications[domain][path] = id_timeout{app_id, timeout}
        this->_applications[app_tmp[0].GetString()].insert(std::make_pair(app_tmp[1].GetString(),
                                                                          id_timeout{app_tmp[2].GetString(),
                                                                                     app_tmp[3].GetUint64()}));
        DARWIN_LOG_DEBUG("Session:: Generator:: Application "+std::to_string(cpt)+" configuration : ["+
                         app_tmp[0].GetString() + "," + app_tmp[1].GetString() +
                         "] => [" + app_tmp[2].GetString() + "," + std::to_string(app_tmp[3].GetUint64()) + "]");
        cpt++;
    }

    redis_socket_path = configuration["redis_socket_path"].GetString();
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    // Done in AlertManager before arriving here, but will allow better transition from redis singleton
    redis.SetUnixConnection(redis_socket_path);
    return redis.FindAndConnect();
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<SessionTask>(socket, manager, _cache, _cache_mutex, _applications));
}
