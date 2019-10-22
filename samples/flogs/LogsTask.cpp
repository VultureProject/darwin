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
                   std::string log_file_path,
                   std::ofstream& log_file,
                   std::string redis_list_name)
        : Session{"logs", socket, manager, cache, cache_mutex},
          _log{log}, _redis{redis},
          _log_file_path{log_file_path}, _log_file{log_file},
          _file_mutex{},
          _redis_list_name{redis_list_name}{}

long LogsTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_LOGS;
}

void LogsTask::operator()() {
    DARWIN_LOGGER;

    if (header.body_size > 0) {
        DARWIN_LOG_DEBUG("LogsTask:: got log: " + body);

        if (_log){
            WriteLogs();
        }
        if (_redis){
            if(not REDISAddLogs(body)) {
                DARWIN_LOG_WARNING("LogsTask:: Could not add log line to Redis.");
            }
        }
    } else {
        DARWIN_LOG_DEBUG("LogsTask:: No log to write, body size: " + std::to_string(header.body_size));
    }

    Workflow();
}

void LogsTask::Workflow() {
    switch (header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendToDarwin();
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
        default:
            break;
    }
}

bool LogsTask::WriteLogs() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("WriteLogsTask::Write:: Starting writing in log file: \""
                     + _log_file_path + "\"...");
    unsigned int retry = RETRY;
    bool fail;

    {
        std::unique_lock<std::mutex> lck{_file_mutex};
        
        fail = !_log_file.is_open() or _log_file.fail();
        while(retry and fail){
            DARWIN_LOG_INFO("LogsGenerator::LoadClassifier:: Error when opening the log file, "
                            "will retry " + std::to_string(retry) + " times");
            _log_file.open(_log_file_path, std::ios::out | std::ios::app);
            fail = !_log_file.is_open() or _log_file.fail();
            retry--;
        }

        if(fail) {
            DARWIN_LOG_ERROR("LogsGenerator::LoadClassifier:: Error when opening the log file, "
                            "too many retry, "
                            "may due to low space disk or wrong permission");
            return false;
        }

        _log_file << body << std::flush;
    }

    return true;
}

bool LogsTask::REDISAddLogs(const std::string& logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask::REDISAdd:: Add logs in Redis...");

    darwin::toolkit::RedisManager& instance = darwin::toolkit::RedisManager::GetInstance();
    return instance.Query(std::vector<std::string>{"LPUSH", _redis_list_name, logs}) != REDIS_REPLY_ERROR;
}

bool LogsTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask:: ParseBody: " + body);
    return true;
}
