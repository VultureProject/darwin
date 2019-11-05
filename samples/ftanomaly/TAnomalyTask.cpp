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
                             std::mutex& cache_mutex,
                             std::shared_ptr<AnomalyThreadManager> vat,
                             std::string redis_list_name)
        : Session{"tanomaly", socket, manager, cache, cache_mutex}, _redis_list_name{std::move(redis_list_name)},
        _anomaly_thread_manager{std::move(vat)}
{
}

long AnomalyTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_TANOMALY;
}

void AnomalyTask::operator()() {
    DARWIN_LOGGER;
    if (_learning_mode){
        _anomaly_thread_manager->Start();
    } else {
        _anomaly_thread_manager->Stop();
    }

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    DARWIN_LOG_DEBUG("AnomalyTask:: got " + std::to_string(array.Size()) + " lines to treat");

    for (auto& line : array) {
        if(ParseLine(line)) {
            REDISAddEntry();
            _certitudes.push_back(0);
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }
}

bool AnomalyTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: parse raw body: " + _raw_body);

    try {
        rapidjson::Document document;
        document.Parse(_raw_body.c_str());

        if (document.IsArray()) {
            DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: Body is an array");
            _body.Swap(document);
            DARWIN_LOG_DEBUG("TAnomalyTask:: ParseBody:: body is " + JsonStringify(_body));
            return true;
        }

        if (!document.IsObject()) {
            DARWIN_LOG_ERROR("AnomalyTask:: ParseBody:: You must provide a valid Json");
            return false;
        }

        //********LEARNING MODE*********//
        if (document.HasMember("learning_mode")){
            DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: Body has learning mode");
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
            DARWIN_LOG_DEBUG("AnomalyTask:: ParseBody:: Body has data");
            rapidjson::Value& data = document["data"];

            if (!data.IsArray()) {
                DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: \"data\" needs to be an array");
                return false;
            }

            _body.SetArray();
            rapidjson::Document::AllocatorType& allocator = _body.GetAllocator();

            for (auto &value : data.GetArray()) {
                _body.PushBack(value, allocator);
            }

            DARWIN_LOG_DEBUG("TAnomalyTask:: ParseBody:: body is " + JsonStringify(_body));
        }
        //*************DATA************//

    } catch (...) {
        DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody:: Unknown Error");
        return false;
    }

    return true;
}

bool AnomalyTask::ParseLine(rapidjson::Value& line){
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("AnomalyTask:: ParseLine:: The input line is not an array");
        return false;
    }

    _entry.clear();
    auto values = line.GetArray();

    for (auto& value : values){

        if (!value.IsString()) {
            DARWIN_LOG_WARNING("AnomalyTask:: ParseLine:: Every entry must be a string");
            return false;
        }
        _entry += value.GetString();
        _entry += ";";
    }

    if(_entry.size() > 0) _entry.pop_back();

    if (!std::regex_match (_entry, std::regex(
            "(((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\."
            "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?);){2})"
            "(([0-9]+;(17|6))|([0-9]*;*1))")))
    {
        DARWIN_LOG_WARNING("AnomalyTask:: ParseLine:: The data: "+ _entry +", isn't valid, ignored. "
                                                                            "Format expected : "
                                                                            "[\"[ip4]\",\"[ip4]\",((\"[port]\","
                                                                            "\"[ip_protocol udp or tcp]\")|"
                                                                            "\"[ip_protocol icmp]\")]");
        return false;
    }

    return true;
}

bool AnomalyTask::REDISAddEntry() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask::REDISAdd:: Add data in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_list_name);
    arguments.emplace_back(_entry);

    if(redis.Query(arguments) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyTask::REDISAdd:: Not the expected Redis response");
        return false;
    }

    return true;
}
