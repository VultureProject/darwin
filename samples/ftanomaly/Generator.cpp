/// \file     Generator.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "base/Logger.hpp"
#include "base/Core.hpp"
#include "Generator.hpp"
#include "TAnomalyTask.hpp"
#include "TAnomalyThreadManager.hpp"

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("TAnomaly:: Generator:: Loading configuration...");

    // Generating name for the internaly used redis set
    _redis_internal = darwin::Core::instance().GetFilterName() + REDIS_INTERNAL_LIST;

    std::string redis_alerts_channel; //the channel on which to publish alerts when detected
    std::string redis_alerts_list; //the list on which to add alerts when detected
    bool start_detection_thread = false;  //whether to start the anomaly thread
    unsigned int detection_frequency = 300; //detection thread launching frequency in seconds

    if(not configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("TAnomaly::Generator:: 'redis_socket_path' parameter missing, mandatory");
        return false;
    }

    if (not configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: 'redis_socket_path' needs to be a string");
        return false;
    }

    std::string redis_socket_path = configuration["redis_socket_path"].GetString();
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

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

    bool is_log_redis = configuration.HasMember("redis_list_name") or configuration.HasMember("redis_channel_name");
    bool is_log_file = configuration.HasMember("log_file_path");

    if (not is_log_redis and not is_log_file){
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Need at least one of these parameters: "
                            "'redis_socket_path' or 'log_file_path'");
        return false;
    }

    if (is_log_redis){
        if (configuration.HasMember("redis_list_name")){
            if (not configuration["redis_list_name"].IsString()) {
                DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: 'redis_list_name' needs to be a string");
                return false;
            }
            redis_alerts_list = configuration["redis_list_name"].GetString();
            DARWIN_LOG_INFO("TAnomaly:: Generator:: 'redis_list_name' set to " + redis_alerts_list);
        }

        if (configuration.HasMember("redis_channel_name")){
            if (not configuration["redis_channel_name"].IsString()) {
                DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: 'redis_channel_name' needs to be a string");
                return false;
            }
            redis_alerts_channel = configuration["redis_channel_name"].GetString();
            DARWIN_LOG_INFO("TAnomaly:: Generator:: 'redis_channel_name' set to " + redis_alerts_channel);
        }

        if(redis_alerts_list.empty() and redis_alerts_channel.empty()) {
            DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: if 'redis_socket_path' is provided, you need to provide at least"
                                    " 'redis_list_name' or 'redis_channel_name', they cannot be empty");
            return false;
        }

        if(not redis_alerts_list.compare(_redis_internal)) {
            redis_alerts_list.clear();
            if(not redis_alerts_channel.empty()) {
                DARWIN_LOG_WARNING("TAnomaly:: Generator:: 'redis_list_name' parameter cannot be set to '" +
                                    std::string(_redis_internal) + "' (forbidden name), parameter ignored.");
            }
            else {
                DARWIN_LOG_ERROR("TAnomaly:: Generator:: 'redis_list_name' parameter cannot be set to '" +
                                std::string(_redis_internal) + "' (forbidden name). No alternative for Redis, aborting.");
                return false;
            }
        }

        DARWIN_LOG_INFO("TAnomaly:: Generator:: Redis configured successfuly");
    }

    if (is_log_file){
        if (!configuration["log_file_path"].IsString()) {
            DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: 'log_file_path' needs to be a string");
            return false;
        }

        std::string log_file_path(configuration["log_file_path"].GetString());

        // Open file and check permissions (and create file if not existing)
        _log_file = std::make_shared<darwin::toolkit::FileManager>(log_file_path, true);
        if(!_log_file->Open()){
            DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Error when opening log file");
            return false;
        }

        DARWIN_LOG_INFO("TAnomaly::Generator:: Log file configured");
    }

    /* Only start the thread if
    -> the thread is connected to master via unix socket (filter is on the master's machine)
    -> or user forced it
    */
    if(start_detection_thread) {
        _anomaly_thread_manager = std::make_shared<AnomalyThreadManager>(_redis_internal, _log_file, redis_alerts_channel, redis_alerts_list);
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