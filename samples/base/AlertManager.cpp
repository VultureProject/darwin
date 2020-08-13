/// \file     AlerManager.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     12/11/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <sstream>
#include "Logger.hpp"
#include "AlertManager.hpp"
#include "Time.hpp"

namespace darwin {

    AlertManager::AlertManager():
    _log{false}, _redis{false}
    {}

    bool AlertManager::Configure(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager:: Configuring...");

        bool redis_status = false;
        bool logs_status = false;

        _redis = configuration.HasMember("redis_socket_path");
        _log = configuration.HasMember("log_file_path");

        if (!_redis and !_log){
            DARWIN_LOG_WARNING("AlertManager:: Need at least one of these parameters to raise alerts: "
                                "'redis_socket_path' or 'log_file_path'");
            return false;
        }

        if (_redis)
            redis_status = this->LoadRedisConfig(configuration);
        if (_log)
            logs_status = this->LoadLogsConfig(configuration);

        // The goal is to return true if everything is ok, false otherwise.
        //
        // Let's take the redis part as an example, the log part works the same way :
        // It verifies if redis is present, if so returns the redis status, returns true otherwise.
        // If a configuration is not present, then everything went right for its part.
        return ((_redis && redis_status) || (!_redis && !redis_status)) && ((_log && logs_status) || (!_log && !logs_status));
    }

    bool AlertManager::LoadLogsConfig(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;

        if (not GetStringField(configuration, "log_file_path", _log_file_path)) {
            _log = false; // Error configuring logs, disabling...
            return false;
        }
        // Open file and check permissions (and create file if not existing)
        _log_file = std::make_shared<darwin::toolkit::FileManager>(_log_file_path, true);
        if(!_log_file->Open()){
            DARWIN_LOG_WARNING("AlertManager:: Error when opening log file. Ignoring log file configuration...");
            _log = false; // Error configuring logs, disabling...
            return false;
        }
        return true;
    }

    bool AlertManager::LoadRedisConfig(const rapidjson::Document& configuration) {
        DARWIN_LOGGER;
        std::string redis_socket_path;

        GetStringField(configuration, "redis_socket_path", redis_socket_path);
        if (redis_socket_path.empty()) {
            DARWIN_LOG_WARNING("AlertManager:: 'redis_socket_path' needs to be a non-empty string. Ignoring REDIS configuration...");
            _redis = false; // Error configuring redis, disabling...
            return false;
        }
        GetStringField(configuration, "alert_redis_list_name", this->_redis_list_name);
        GetStringField(configuration, "alert_redis_channel_name", this->_redis_channel_name);
        if(_redis_list_name.empty() and _redis_channel_name.empty()) {
            DARWIN_LOG_WARNING("AlertManager:: if 'redis_socket_path' is provided, you need to provide at least"
                                    " 'alert_redis_list_name' or 'alert_redis_channel_name'."
                                    "Unable to configure REDIS. Ignoring REDIS configuration...");
            _redis = false; // Error configuring redis, disabling...
            return false;
        }
        if(!ConfigRedis(redis_socket_path)){
            DARWIN_LOG_WARNING("AlertManager:: Error when configuring REDIS. Ignoring REDIS configuration...");
            _redis = false; // Error configuring redis, disabling...
            return false;
        }
        return true;
    }

    bool AlertManager::GetStringField(const rapidjson::Document& configuration,
                                      const char* const field_name,
                                      std::string& var) {
        DARWIN_LOGGER;

        if (configuration.HasMember(field_name)){
            if (configuration[field_name].IsString() && configuration[field_name].GetStringLength() > 0) {
                var = configuration[field_name].GetString();
                DARWIN_LOG_INFO(std::string("AlertManager:: '") + field_name + "' set to " + var);
                return true;
            } else {
                DARWIN_LOG_WARNING(std::string("AlertManager:: '") + field_name + "' needs to be a non-empty string."
                                    "Ignoring this field...");
            }
        }
        return false;
    }

    bool AlertManager::ConfigRedis(std::string redis_socket_path) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager:: Redis configuration...");

        darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
        redis.SetUnixConnection(redis_socket_path);
        return redis.FindAndConnect();
    }

    void AlertManager::Alert(const std::string& str) {
        if (str.length() <= 0)
            return;
        if (_log)
            this->WriteLogs(str);
        if (_redis)
            this->REDISAddLogs(str);
    }

    void AlertManager::Alert(const std::string& entry,
               const unsigned int certitude,
               const std::string& evt_id,
               const std::string& details,
               const std::string& tags) {
        this->Alert(this->FormatLog(entry, certitude, evt_id, details, tags));
    }

    bool AlertManager::WriteLogs(const std::string& str) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager::WriteLogs:: Starting to write in log file: '"
                        + _log_file_path + "'...");
        unsigned int retry = RETRY;
        bool fail;

        fail = !(_log_file->Write(str + '\n'));
        while(retry and fail){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            DARWIN_LOG_INFO("AlertManager::WriteLogs:: Error when writing in log file, "
                            "will retry " + std::to_string(retry) + " times");
            fail = !(_log_file->Write(str + '\n'));
            retry--;
        }
        if(fail) {
            DARWIN_LOG_ERROR("AlertManager::WriteLogs:: Error when writing in log file, "
                            "too many retry, "
                            "may be due to low space disk or wrong permission, see stderr");
            return false;
        }
        return true;
    }
#include <unistd.h>
    bool AlertManager::REDISAddLogs(const std::string& logs) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("AlertManager::REDISAddLogs:: Add logs in Redis...");

        darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

        if(not _redis_list_name.empty()) {
            if(redis.Query(std::vector<std::string>{"LPUSH", _redis_list_name, logs}, true) == REDIS_REPLY_ERROR) {
                DARWIN_LOG_WARNING("AlertManager::REDISAddLogs:: Failed to add log in Redis !");
                return false;
            }
        }

        if(not _redis_channel_name.empty()) {
            if(redis.Query(std::vector<std::string>{"PUBLISH", _redis_channel_name, logs}, true) == REDIS_REPLY_ERROR) {
                DARWIN_LOG_WARNING("AlertManager::REDISAddLogs:: Failed to publish log in Redis !");
                return false;
            }
        }
        return true;
    }

    void AlertManager::Rotate() {
        this->_log_file->Open(true);
    }

    std::string AlertManager::FormatLog(const std::string& entry,
                                        const unsigned int certitude,
                                        const std::string& evt_id,
                                        const std::string& details,
                                        const std::string& tags) const {
        std::stringstream ss;

        ss << "{\"alert_type\": \"darwin\", ";
        ss << "\"alert_subtype\": \"" << this->_filter_name << "\", ";
        ss << "\"alert_time\": \"" << darwin::time_utils::GetTime() << "\", ";
        ss << "\"level\": \"high\", ";
        ss << "\"rule_name\": \"" << this->_rule_name << "\", ";

        if (tags.empty())
            ss << "\"tags\": " << this->_tags << ", ";
        else
            ss << "\"tags\": " << tags << ", ";

        ss << "\"entry\": \"" << entry << "\", ";
        ss << "\"score\": "  << certitude << ", ";
        ss << "\"evt_id\": \"" << evt_id << "\", ";
        ss << "\"details\": ";
        ss << details;
        ss << "}";
        return ss.str();
    }
}
