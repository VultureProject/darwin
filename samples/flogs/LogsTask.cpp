/// \file     LogsTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>

#include "Logger.hpp"
#include "Stats.hpp"
#include "protocol.h"
#include "LogsTask.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../toolkit/rapidjson/document.h"

LogsTask::LogsTask(boost::asio::local::stream_protocol::socket& socket,
                   darwin::Manager& manager,
                   std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                   std::mutex& cache_mutex,
                   bool log,
                   bool redis,
                   std::string& log_file_path,
                   std::shared_ptr<darwin::toolkit::FileManager>& log_file,
                   std::string& redis_list_name,
                   std::string& redis_channel_name)
        : Session{"logs", socket, manager, cache, cache_mutex},
          _log{log}, _redis{redis},
          _log_file_path{log_file_path},
          _redis_list_name{redis_list_name},
          _redis_channel_name{redis_channel_name},
          _log_file{log_file} {}

long LogsTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_LOGS;
}

void LogsTask::operator()() {
    DARWIN_LOGGER;

    if (_header.body_size > 0) {
        DARWIN_LOG_DEBUG("LogsTask:: got log: " + _raw_body);
        STAT_INPUT_INC;

        if (_log){
            WriteLogs();
        }
        if (_redis){
            if(not REDISAddLogs(_raw_body)) {
                DARWIN_LOG_WARNING("LogsTask:: Could not add log line to Redis.");
            }
        }
    } else {
        DARWIN_LOG_DEBUG("LogsTask:: No log to write, body size: " + std::to_string(_header.body_size));
    }
}

bool LogsTask::WriteLogs() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("WriteLogsTask::WriteLogs:: Starting writing in log file: \""
                     + _log_file_path + "\"...");
    unsigned int retry = RETRY;
    bool fail;

    fail = !(_log_file->Write(_raw_body));
    while(retry and fail){
        DARWIN_LOG_INFO("LogsGenerator::WriteLogs:: Error when writing in log file, "
                        "will retry " + std::to_string(retry) + " times");
        fail = !(_log_file->Write(_raw_body));
        retry--;
    }

    if(fail) {
        DARWIN_LOG_ERROR("LogsGenerator::WriteLogs:: Error when writing in log file, "
                         "too many retry, "
                         "may due to low space disk or wrong permission, see stderr");
        return false;
    }

    return true;
}

bool LogsTask::REDISAddLogs(const std::string& logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask::REDISAddLogs:: Add logs in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(not _redis_list_name.empty()) {
        if(redis.Query(std::vector<std::string>{"LPUSH", _redis_list_name, logs}) == REDIS_REPLY_ERROR) {
            DARWIN_LOG_WARNING("LogsTask::REDISAddLogs:: Failed to add log in Redis !");
            return false;
        }
    }

    if(not _redis_channel_name.empty()) {
        if(redis.Query(std::vector<std::string>{"PUBLISH", _redis_channel_name, logs}) == REDIS_REPLY_ERROR) {
            DARWIN_LOG_WARNING("LogsTask::REDISAddLogs:: Failed to publish log in Redis !");
            return false;
        }
    }
    return true;
}

bool LogsTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask:: ParseBody: entry to log = '" + _raw_body + "'");
    return true;
}
