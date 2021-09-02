/// \file     ConnectionSupervisionTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <regex>
#include <string>
#include <thread>
#include <string.h>
#include <config.hpp>

#include "Logger.hpp"
#include "Stats.hpp"
#include "AlertManager.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "ConnectionSupervisionTask.hpp"
#include "ASession.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"


ConnectionSupervisionTask::ConnectionSupervisionTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                                    std::mutex& cache_mutex,
                                                    darwin::session_ptr_t s,
                                                    darwin::DarwinPacket& packet,
                                                    unsigned int expire)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet),
          _redis_expire{expire}{}

long ConnectionSupervisionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_CONNECTION;
}

void ConnectionSupervisionTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = _s->GetOutputType() == darwin::config::output_type::LOG;
    auto logs = _packet.GetMutableLogs();
    unsigned int certitude;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        STAT_INPUT_INC;
        SetStartingTime();

        if(ParseLine(line)) {
            certitude = REDISLookup(_connection);

            if(certitude >= _threshold and certitude < DARWIN_ERROR_RETURN){
                STAT_MATCH_INC;
                DARWIN_ALERT_MANAGER.Alert(_connection, certitude, _packet.Evt_idToString());
                if (is_log) {
                    std::string alert_log = R"({"evt_id": ")" + _packet.Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() + R"(", "filter": ")" + GetFilterName() +
                            R"(", "connection": ")" + _connection + R"(", "certitude": )" + std::to_string(certitude) + "}";
                    logs += alert_log + "\n";
                }
            }

            _packet.AddCertitude(certitude);
            DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            STAT_PARSE_ERROR_INC;
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }
    }
}

bool ConnectionSupervisionTask::ParseLine(rapidjson::Value& line){
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("ConnectionSupervisionTask:: ParseBody: The input line is not an array");
        return false;
    }

    _connection.clear();
    auto values = line.GetArray();

    for (auto& a: values){
        if (!a.IsString()) {
            DARWIN_LOG_WARNING("ConnectionSupervisionTask:: ParseLine:: Not string type encounter in data, ignored");
            continue;
        }
        _connection += a.GetString();
        _connection += ";";
    }

    if(!_connection.empty()) _connection.pop_back();

    if (!std::regex_match (_connection, std::regex(
            "(((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\."
            "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?);){2})"
            "(([0-9]+;(17|6))|([0-9]*;*1))")))
    {
        DARWIN_LOG_WARNING("ConnectionSupervisionTask:: ParseLine:: The data: "+ _connection +", isn't valid, ignored. "
                                                                                        "Format expected : "
                                                                                        "[\\\"[ip4]\\\";\\\"[ip4]\\\";((\\\"[port]\\\";"
                                                                                        "\\\"[ip_protocol udp or tcp]\\\")|"
                                                                                        "\\\"[ip_protocol icmp]\\\")]");
        return false;
    }
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ParseLine: Parsed element: " + _connection);

    return true;
}

unsigned int ConnectionSupervisionTask::REDISLookup(const std::string& connection) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: Looking up '" +  connection  + "' in the Redis");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    long long int result;

    std::vector<std::string> arguments{};
    arguments.emplace_back("EXISTS");
    arguments.emplace_back(connection);

    if(redis.Query(arguments, result, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Didn't get the expected response from Redis"
                        " when looking for connection '" + connection + "'.");
        return DARWIN_ERROR_RETURN;
    }

    // When doubt, everything is ok
    unsigned int certitude = 0;

    if (not result) {
        DARWIN_LOG_DEBUG("ConnectionSupervisionTask::REDISLookup:: No result found, setting certitude to 100");
        certitude = 100;

        // If connection not found in Redis, add it
        arguments.clear();
        if(_redis_expire){
            arguments.emplace_back("SETEX");
            arguments.emplace_back(connection);
            arguments.emplace_back(std::to_string(_redis_expire));
        } else{
            arguments.emplace_back("SET");
            arguments.emplace_back(connection);
        }
        arguments.emplace_back("0");

        if(redis.Query(arguments, true) == REDIS_REPLY_ERROR) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Something went wrong "
                             "while adding a new connection to Redis");
            return DARWIN_ERROR_RETURN;
        }
    }

    DARWIN_LOG_DEBUG("ConnectionSupervisionTask::REDISLookup:: Certitude of a new connection is " +
                     std::to_string(certitude));

    return certitude;
}
