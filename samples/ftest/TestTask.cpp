/// \file     TestTask.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     11/12/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "TestTask.hpp"
#include "Logger.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

TestTask::TestTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               std::string& redis_list,
                               std::string& redis_channel)
        : Session{"test", socket, manager, cache, cache_mutex},
        _redis_list{redis_list},
        _redis_channel{redis_channel} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t TestTask::GenerateHash() {
    return xxh::xxhash<64>(_line, _line.length());
}

long TestTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_TEST;
}

void TestTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        SetStartingTime();
        xxh::hash64_t hash;

        if(ParseLine(line)) {
            if(_line == "trigger_redis_list") {
                if(REDISAddList(_redis_list, _line)) {
                    _certitudes.push_back(0);
                }
                else {
                    _certitudes.push_back(DARWIN_ERROR_RETURN);
                }
            }
            else if(_line == "trigger_redis_channel") {
                if(REDISPublishChannel(_redis_channel, _line)) {
                    _certitudes.push_back(0);
                }
                else {
                    _certitudes.push_back(DARWIN_ERROR_RETURN);
                }
            }
            else {
                DARWIN_ALERT_MANAGER.Alert(_line, 100, Evt_idToString());
                _certitudes.push_back(0);
            }
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }
}

bool TestTask::REDISAddList(const std::string& list, const std::string& line) {
    DARWIN_LOGGER;

    DARWIN_LOG_INFO("TestTask::REDISAddList:: adding '" + line + "' to list '" + list + "'");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"LPUSH", list, line}, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("LogsTask::REDISAddLogs:: Failed to add log in Redis !");
        return false;
    }

    return true;
}

bool TestTask::REDISPublishChannel(const std::string& channel, const std::string& line) {
    DARWIN_LOGGER;

    DARWIN_LOG_INFO("TestTask::REDISPublishChannel:: publishing '" + line + "' to channel '" + channel + "'");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"PUBLISH", channel, line}, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("LogsTask::REDISAddLogs:: Failed to add log in Redis !");
        return false;
    }

    return true;
}

bool TestTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: You must provide only one value in the list");
        return false;
    }

    if (not values[0].IsString()) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: The value sent must be a string");
        return false;
    }

    _line = values[0].GetString();

    if (_line == "trigger_ParseLine_error")
        return false;

    return true;
}
