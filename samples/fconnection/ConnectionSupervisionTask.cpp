/// \file     ConnectionSupervisionTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <config.hpp>
#include <string>
#include <string.h>
#include <thread>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "ConnectionSupervisionTask.hpp"
#include "Logger.hpp"
#include "protocol.h"

ConnectionSupervisionTask::ConnectionSupervisionTask(boost::asio::local::stream_protocol::socket& socket,
                                                     darwin::Manager& manager,
                                                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                                     std::shared_ptr<darwin::toolkit::RedisManager> rm,
                                                     unsigned int expire)
        : Session{socket, manager, cache}, _redis_expire{expire}, _redis_manager{rm} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t ConnectionSupervisionTask::GenerateHash() {
    return xxh::xxhash<64>(_current_connection);
}

long ConnectionSupervisionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_CONNECTION;
}

void ConnectionSupervisionTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    for (const std::string &connection : _connections) {
        SetStartingTime();
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current connection being processed, to compute the hash
        // accordingly
        _current_connection = connection;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + R"(", "connection": ")" + connection +
                             R"(", "certitude": )" + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                continue;
            }
        }

        certitude = REDISLookup(connection);
        if(certitude!=100){
            if (is_log && certitude){
                _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + R"(", "connection": ")" + connection +
                         R"(", "certitude": )" + std::to_string(certitude) + "}\n";
            }

            _certitudes.push_back(certitude);

            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
        }

        DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _connections = std::vector<std::string>();
}

std::string ConnectionSupervisionTask::GetTime(){
    char str_time[256];
    time_t rawtime;
    struct tm * timeinfo;
    std::string res;

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(str_time, sizeof(str_time), "%F%Z%T%z", timeinfo);
    res = str_time;

    return res;
}

void ConnectionSupervisionTask::Workflow() {
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

bool ConnectionSupervisionTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ParseBody: " + body);
    std::string connection;
    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            connection="";
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("ConnectionSupervisionTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            for (auto& s: request.GetArray()) {
                if (!s.IsString()) {
                    DARWIN_LOG_ERROR("ConnectionSupervisionTask:: ParseBody: The connection sent must be a string");
                    return false;
                }
                connection+=s.GetString();
            }
            _connections.push_back(connection);
            DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ParseBody: Parsed element: " + connection);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("ConnectionSupervisionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

unsigned int ConnectionSupervisionTask::REDISLookup(const std::string& connection) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: Looking up '" +  connection  + "' in the Redis");

    redisReply *reply = nullptr;

    std::vector<std::string> arguments;
    arguments.emplace_back("EXISTS");
    arguments.emplace_back(connection);

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        return 101;
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

        if (!_redis_manager->REDISQuery(&reply, arguments)) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask::REDISLookup:: Something went wrong "
                             "while adding a new connection to Redis");
            freeReplyObject(reply);
            return 101;
        }
    }

    freeReplyObject(reply);
    reply = nullptr;

    DARWIN_LOG_DEBUG("ConnectionSupervisionTask::REDISLookup:: Certitude of a new connection is " +
                     std::to_string(certitude));

    return certitude;
}
