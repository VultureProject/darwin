/// \file     Generator.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "base/Core.hpp"
#include "Generator.hpp"
#include "TAnomalyTask.hpp"
#include "TAnomalyThreadManager.hpp"
#include "AlertManager.hpp"

bool Generator::ConfigureAlterting(const std::string& tags) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Hostlookup:: ConfigureAlerting:: Configuring Alerting");
    DARWIN_ALERT_MANAGER_SET_FILTER_NAME(DARWIN_FILTER_NAME);
    DARWIN_ALERT_MANAGER_SET_RULE_NAME(DARWIN_ALERT_RULE_NAME);
    if (tags.empty()) {
        DARWIN_LOG_DEBUG("Hostlookup:: ConfigureAlerting:: No alert tags provided in the configuration. Using default.");
        DARWIN_ALERT_MANAGER_SET_TAGS(DARWIN_ALERT_TAGS);
    } else {
        DARWIN_ALERT_MANAGER_SET_TAGS(tags);
    }
    return true;
}

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("TAnomaly:: Generator:: Loading configuration...");

    _redis_internal = darwin::Core::instance().GetFilterName() + REDIS_INTERNAL_LIST; // Generating name for the internaly used redis set
    bool start_detection_thread = false;  //whether to start the anomaly thread
    unsigned int detection_frequency = 300; //detection thread launching frequency in seconds
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(not configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("TAnomaly::Generator:: 'redis_socket_path' parameter missing, mandatory");
        return false;
    }
    if (not configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: 'redis_socket_path' needs to be a string");
        return false;
    }
    std::string redis_socket_path = configuration["redis_socket_path"].GetString();
    redis.SetUnixConnection(redis_socket_path);
    // Done in AlertManager before arriving here, but will allow better transition from redis singleton
    if(not redis.FindAndConnect()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Could not connect to a redis!");
        return false;
    }

    // start detection thread if the local redis server is a master
    if(darwin::toolkit::RedisManager::TestIsMaster(redis_socket_path)) {
        DARWIN_LOG_INFO("TAnomaly::Generator:: direct connection to redis master found, detection thread will be started");
        start_detection_thread = true;
    }
    // Override start_detection_thread with parameter
    if(configuration.HasMember("start_detection_thread") and configuration["start_detection_thread"].IsBool()) {
        start_detection_thread = configuration["detect"].GetBool();
        DARWIN_LOG_INFO("TAnomaly:: Generator:: 'start_detection_thread' specified, "
                        + std::string((start_detection_thread ? "enabling" : "disabling")) + " detection");
    }

    if(configuration.HasMember("detection_frequency") and configuration["detection_frequency"].IsInt()) {
        detection_frequency = configuration["detection_frequency"].GetUint64();
        DARWIN_LOG_INFO("TAnomaly:: Generator:: detection frequency manually set to "
                            + std::to_string(detection_frequency));
    }

    /* Only start the thread if
    -> the thread is connected to master via unix socket (filter is on the master's machine)
    -> or user forced it
    */
    if(start_detection_thread) {
        _anomaly_thread_manager = std::make_shared<AnomalyThreadManager>(_redis_internal);
        if(!_anomaly_thread_manager->Start(detection_frequency)) {
            DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Error when starting polling thread");
            return false;
        }
    }
    else {
        DARWIN_LOG_INFO("TAnomaly:: Generator:: detection thread is disabled (or no direct connection to redis master"
         " was found) so detection will not start");
    }
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<AnomalyTask>(socket, manager, _cache, _cache_mutex,
                                          _anomaly_thread_manager, _redis_internal));
}