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
#include "protocol.h"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "ConnectionSupervisionTask.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"


ConnectionSupervisionTask::ConnectionSupervisionTask(boost::asio::local::stream_protocol::socket& socket,
                                                     darwin::Manager& manager,
                                                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                                     std::shared_ptr<darwin::toolkit::RedisManager> rm,
                                                     unsigned int expire)
        : Session{"connection", socket, manager, cache},
          _redis_expire{expire}, _redis_manager{std::move(rm)} {}

long ConnectionSupervisionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_CONNECTION;
}

void ConnectionSupervisionTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;
    unsigned int certitude;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        SetStartingTime();

        if(ParseLine(line)) {
            certitude = REDISLookup(_connection);

            if(is_log && certitude>=_threshold){
                _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() + R"(", "filter": ")" + GetFilterName() +
                        R"(", "connection": ")" + _connection + R"(", "certitude": )" + std::to_string(certitude) + "}\n";
            }

            _certitudes.push_back(certitude);
            DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }

    Workflow();
}

void ConnectionSupervisionTask::Workflow() {
    switch (_header.response) {
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

bool ConnectionSupervisionTask::ParseLine(rapidjson::Value& line){
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("DGATask:: ParseBody: The input line is not an array");
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
                                                                                        "[\"[ip4]\";\"[ip4]\";((\"[port]\";"
                                                                                        "\"[ip_protocol udp or tcp]\")|"
                                                                                        "\"[ip_protocol icmp]\")]");
        return false;
    }
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ParseLine: Parsed element: " + _connection);

    return true;
}

unsigned int ConnectionSupervisionTask::REDISLookup(const std::string& connection) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: Looking up '" +  connection  + "' in the Redis");

    redisReply *reply = nullptr;

    std::vector<std::string> arguments{};
    arguments.emplace_back("EXISTS");
    arguments.emplace_back(connection);

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        reply = nullptr;
        return DARWIN_ERROR_RETURN;
    }

    // When doubt, everything is ok
    unsigned int certitude = 0;

    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Not the expected Redis response "
                         "when looking up for connection " + connection);
    } else if (!reply->integer) {
        certitude = 100;

        // If connection not found in the Redis, we put it in
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

        freeReplyObject(reply);
        reply = nullptr;
        if (!_redis_manager->REDISQuery(&reply, arguments)) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Something went wrong "
                             "while adding a new connection to Redis");
            freeReplyObject(reply);
            reply = nullptr;
            return DARWIN_ERROR_RETURN;
        }
    }

    freeReplyObject(reply);
    reply = nullptr;

    DARWIN_LOG_DEBUG("ConnectionSupervisionTask::REDISLookup:: Certitude of a new connection is " +
                     std::to_string(certitude));

    return certitude;
}
