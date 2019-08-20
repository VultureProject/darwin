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
                   bool log,
                   bool redis,
                   std::string log_file_path,
                   std::string redis_list_name,
                   std::shared_ptr<darwin::toolkit::RedisManager> redis_manager)
        : Session{socket, manager, cache},
          _log{log}, _redis{redis},
          _log_file_path{log_file_path}, _redis_list_name{redis_list_name},
          _redis_manager{redis_manager}{}

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
            REDISAddLogs(body);
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

    std::ofstream logFile(_log_file_path, std::ios::out | std::ios::app);

    if (!logFile.is_open() or logFile.fail()) {
        DARWIN_LOG_ERROR("WriteLogsTask::Write:: Error when opening the log file, "
                         "maybe too low space disk or wrong permission");
        return false;
    }

    logFile << body;
    logFile.close();

    return true;
}

bool LogsTask::REDISAddLogs(const std::string& logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask::REDISAdd:: Add logs in Redis...");

    redisReply *reply = nullptr;

    std::vector<std::string> arguments;
    arguments.emplace_back("LPUSH");
    arguments.emplace_back(_redis_list_name);
    arguments.emplace_back(logs);

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("LogsTask::REDISAdd:: Something went wrong while querying Redis, data not added");
        freeReplyObject(reply);
        return false;
    }

    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("LogsTask::REDISAdd:: Not the expected Redis response");
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;

    return true;
}

bool LogsTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("LogsTask:: ParseBody: " + body);
    return true;
}
