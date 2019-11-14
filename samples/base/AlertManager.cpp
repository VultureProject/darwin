/// \file     AlerManager.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     12/11/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include "Logger.hpp"
#include "AlertManager.hpp"

namespace darwin {

    bool AlertManager::Configure(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager:: Configuring...");

        bool redis_status = false;
        bool logs_status = false;

        _redis = configuration.HasMember("redis_socket_path");
        _log = configuration.HasMember("log_file_path");

        if (!_redis and !_log){
            DARWIN_LOG_WARNING("AlertManager:: Need at least one of these parameters to raise alerts: "
                                "\"redis_socket_path\" or \"log_file_path\"");
            return false;
        }

        if (_redis)
            redis_status = this->LoadRedisConfig(configuration);
        if (_log)
            logs_status = this->LoadLogsConfig(configuration);

        // This needs some explanation. The goal is to return true if everything is ok, false otherwise.
        //
        // Let's take the redis part as example, the log part works the same way :
        // It verifies if redis is present, if so returns the redis status, returns true otherwise.
        // If a configuration is not present, then everything went right for it's part.
        return ((_redis && redis_status) || (!_redis && !redis_status)) && ((_log && logs_status) || (!_log && !logs_status));
    }

    bool AlertManager::LoadLogsConfig(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;

        if (not GetStringField(configuration, "log_file_path", _log_file_path))
            return false;
        // Open file and check permissions (and create file if not existing)
        _log_file = std::make_shared<darwin::toolkit::FileManager>(_log_file_path, true);
        if(!_log_file->Open()){
            DARWIN_LOG_CRITICAL("AlertManager:: Error when opening log file");
            return false;
        }
    }

    bool AlertManager::LoadRedisConfig(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;

        if (!configuration["redis_socket_path"].IsString()) {
            DARWIN_LOG_WARNING("AlertManager:: \"redis_socket_path\" needs to be a string. Ignoring redis configuration.");
            return false;
        }
        std::string redis_socket_path = configuration["redis_socket_path"].GetString();

        GetStringField(configuration, "redis_list_name", this->_redis_list_name);
        GetStringField(configuration, "redis_channel_name", this->_redis_channel_name);

        if(_redis_list_name.empty() and _redis_channel_name.empty()) {
            DARWIN_LOG_WARNING("AlertManager:: if \"redis_socket_path\" is provided, you need to provide at least"
                                    " \"redis_list_name\" or \"redis_channel_name\"."
                                    "Unable to configure redis. Ignoring redis configuration.");
            return false;
        }

        if(!ConfigRedis(redis_socket_path)){
            DARWIN_LOG_CRITICAL("AlertManager:: Error when configuring REDIS");
            return false;
        }
    }

    bool AlertManager::GetStringField(const rapidjson::Document& configuration,
                                      const char* const field_name,
                                      std::string& var) {
        DARWIN_LOGGER;

        if (configuration.HasMember(field_name)){
            if (configuration[field_name].IsString()) {
                var = configuration[field_name].GetString();
                DARWIN_LOG_INFO(std::string("AlertManager:: \"") + field_name + "\" set to " + var);
            } else {
                DARWIN_LOG_WARNING(std::string("AlertManager:: \"") + field_name + "\" needs to be a string."
                                    "Ignoring this field.");
            }
        }
    }

    bool AlertManager::ConfigRedis(std::string redis_socket_path) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager:: Redis configuration...");

        darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
        bool ret = redis.SetUnixPath(redis_socket_path);

        return ret;
    }

}