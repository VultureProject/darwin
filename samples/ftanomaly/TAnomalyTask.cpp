/// \file     AnomalyTask.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <thread>
#include <string>
#include <regex>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "TAnomalyTask.hpp"
#include "Logger.hpp"
#include "protocol.h"

AnomalyTask::AnomalyTask(boost::asio::local::stream_protocol::socket& socket,
                             darwin::Manager& manager,
                             std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                             std::shared_ptr<darwin::toolkit::RedisManager> redis_manager,
                             std::shared_ptr<AnomalyThreadManager> vat,
                             std::string redis_list_name)
        : Session{socket, manager, cache}, _redis_list_name{std::move(redis_list_name)},
          _redis_manager{std::move(redis_manager)}, _anomaly_thread_manager{std::move(vat)}
{
}

long AnomalyTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_TANOMALY;
}

void AnomalyTask::operator()() {
    if (_learning_mode){
        _anomaly_thread_manager->Start();
    } else {
        _anomaly_thread_manager->Stop();
    }
}

bool AnomalyTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: parse body");

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (document.IsArray()) {
            DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: Body is an array");
            if(!ParseData(document)){
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: Error when parsing \"data\"");
                return false;
            }
            return true;
        }

        if (!document.IsObject()) {
            DARWIN_LOG_ERROR("AnomalyTask:: ParseBody:: You must provide a valid Json");
            return false;
        }

        //********LEARNING MODE*********//
        if (document.HasMember("learning_mode")){
            if (!document["learning_mode"].IsString()) {
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: \"learning_mode\" needs to be a string");
                return false;
            }

            std::string learning_mode = document["learning_mode"].GetString();

            if (learning_mode=="on"){
                _learning_mode = true;
            } else if (learning_mode=="off"){
                _learning_mode = false;
            } else {
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: \"learning_mode\" has to be either \"on\" or \"off\"");
                return false;
            }
        }
        //********LEARNING MODE*********//

        //*************DATA************//
        if (document.HasMember("data")){
            const rapidjson::Value& data = document["data"];

            if (!data.IsArray()) {
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: \"data\" needs to be an array");
                return false;
            }

            if(!ParseData(data)){
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: Error when parsing \"data\"");
                return false;
            }
        }
        //*************DATA************//

    } catch (...) {
        DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: Unknown Error");
        return false;
    }

    return true;
}

bool AnomalyTask::ParseData(const rapidjson::Value& data){
    DARWIN_LOGGER;
    std::string arg, line;
    std::vector<std::string> values;

    for (auto& d: data.GetArray()){
        line = "";

        if (!d.IsArray()) {
            DARWIN_LOG_WARNING("AnomalyTask:: ParseLogs:: Not array type encounter in data, ignored");
            continue;
        }

        for (auto& a: d.GetArray()){
            if (!a.IsString()) {
            DARWIN_LOG_WARNING("AnomalyTask:: ParseLogs:: Not string type encounter in data, ignored");
            continue;
            }
            line += a.GetString();
            line += ";";
        }

        if(line.size() > 0) line.pop_back();

        if (!std::regex_match (line, std::regex(
                "(((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\."
                "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?);){2})"
                "(([0-9]+;(17|6))|([0-9]*;*1))")))
        {
            DARWIN_LOG_WARNING("AnomalyTask:: ParseLogs:: The data: "+ line +", isn't valid, ignored."
                                                                               "Format expected : "
                                                                               "[\"[ip4]\",\"[ip4]\",((\"[port]\","
                                                                               "\"[ip_protocol udp or tcp]\")|"
                                                                               "\"[ip_protocol icmp]\")]");
            continue;
        }
        values.emplace_back(line);
    }

    REDISAdd(values);
    return true;
}

bool AnomalyTask::REDISAdd(std::vector<std::string> values) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask::REDISAdd:: Add data in Redis...");

    redisReply *reply = nullptr;

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_list_name);

    for (std::string& value: values){
        arguments.emplace_back(value);
    }

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("AnomalyTask::REDISAdd:: Something went wrong while querying Redis, data not added");
        freeReplyObject(reply);
        return false;
    }

    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyTask::REDISAdd:: Not the expected Redis response");
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;

    return true;
}
