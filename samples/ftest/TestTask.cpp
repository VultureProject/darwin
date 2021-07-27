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
#include "ASession.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"
#include "Core.hpp"

TestTask::TestTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               darwin::session_ptr_t s,
                               darwin::DarwinPacket& packet,
                               std::string& redis_list,
                               std::string& redis_channel)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet),
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

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        SetStartingTime();

        if(ParseLine(line)) {
            DARWIN_LOG_DEBUG("TestTask:: parsed line successfully");
            if(_line == "trigger_redis_list") {
                DARWIN_LOG_DEBUG("TestTask:: triggered redis list action");
                if(REDISAddList(_redis_list, _line)) {
                    _packet.AddCertitude(0);
                }
                else {
                    _packet.AddCertitude(DARWIN_ERROR_RETURN);
                }
            }
            else if(_line == "trigger_redis_channel") {
                DARWIN_LOG_DEBUG("TestTask:: triggered redis channel action");
                if(REDISPublishChannel(_redis_channel, _line)) {
                    _packet.AddCertitude(0);
                }
                else {
                    _packet.AddCertitude(DARWIN_ERROR_RETURN);
                }
            }
            else if (_line == "load_test") {
                DARWIN_LOG_DEBUG("TestTask:: load_test triggered, doing work for some times (2 000 000 mults)");
                int n = _line.size();
                for(int i=0; i< 2000000;i++){
                    n *=i;
                }
                _packet.AddCertitude(n & 1);
            } else {
                DARWIN_LOG_DEBUG("TestTask:: not triggered specific action, generating alert by default");
                DARWIN_ALERT_MANAGER.Alert(_line, 100, _packet.Evt_idToString(), "{\"filter_name\":\"" + darwin::Core::instance().GetName() + "\"}");
                _packet.AddCertitude(0);
                _response_body.append(_packet.GetBody());
            }
        }
        else {
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }
    }
}

bool TestTask::REDISAddList(const std::string& list, const std::string& line) {
    DARWIN_LOGGER;

    DARWIN_LOG_INFO("TestTask::REDISAddList:: adding '" + line + "' to list '" + list + "'");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"LPUSH", list, line}, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("TestTask::REDISAddLogs:: Failed to add log in Redis !");
        return false;
    }

    return true;
}

bool TestTask::REDISPublishChannel(const std::string& channel, const std::string& line) {
    DARWIN_LOGGER;

    DARWIN_LOG_INFO("TestTask::REDISPublishChannel:: publishing '" + line + "' to channel '" + channel + "'");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"PUBLISH", channel, line}, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("TestTask::REDISAddLogs:: Failed to add log in Redis !");
        return false;
    }

    return true;
}

bool TestTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;

    _line.clear();

    // Parse objects as string
    if (line.IsArray() or line.IsObject()) {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        line.Accept(writer);
        _line = std::string(buffer.GetString());
    }
    else if (line.IsString()){
        _line = line.GetString();
    }
    else if (line.IsNumber()) {
        _line = std::to_string(line.GetDouble());
    }

    DARWIN_LOG_DEBUG("TestTask::ParseLine:: line is '" + _line + "'");

    if (_line == "trigger_ParseLine_error")
        return false;

    return true;
}
