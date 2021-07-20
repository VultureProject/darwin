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
#include "ASession.hpp"
#include "Logger.hpp"
#include "Stats.hpp"

AnomalyTask::AnomalyTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                             std::mutex& cache_mutex,
                             darwin::session_ptr_t s,
                             darwin::DarwinPacket& packet,
                             std::shared_ptr<AnomalyThreadManager> vat,
                             std::string redis_list_name)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet), _redis_list_name{std::move(redis_list_name)},
        _anomaly_thread_manager{std::move(vat)}
{
}

long AnomalyTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_TANOMALY;
}

void AnomalyTask::operator()() {
    DARWIN_LOGGER;
    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    DARWIN_LOG_DEBUG("TAnomalyTask:: got " + std::to_string(array.Size()) + " lines to treat");

    for (auto& line : array) {
        STAT_INPUT_INC;
        if(ParseLine(line)) {
            REDISAddEntry();
            _packet.AddCertitude(0);
        }
        else {
            STAT_PARSE_ERROR_INC;
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }
    }
}

bool AnomalyTask::ParseLine(rapidjson::Value& line){
    DARWIN_LOGGER;
    std::unordered_set<std::string> valid_protos = {"1", "6", "17"};

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("TAnomalyTask:: ParseLine:: The input line is not an array");
        return false;
    }

    _entry.clear();
    auto values = line.GetArray();

    if(values.Size() != 4) {
        DARWIN_LOG_WARNING("TanomalyTask:: ParseLine:: Line should have 4 entries: [ip_src, ip_dst, port, proto]");
        return false;
    }

    for (auto& value : values){

        // all values should be strings
        if(not value.IsString()) {
            DARWIN_LOG_WARNING("TAnomalyTask:: ParseLine:: Every entry must be a string");
            return false;
        }

        std::string value_str = value.GetString();
        // no value should be empty
        if(value_str.empty()) {
            DARWIN_LOG_WARNING("TAnomalyTask:: ParseLine:: entry in line should not be empty");
            return false;
        }

        // all values should NOT contain any ';'
        if(value_str.find(';') != std::string::npos) {
            DARWIN_LOG_WARNING("TAnomalyTask:: ParseLine:: forbidden ';' in entry");
            return false;
        }
        _entry += value_str;
        _entry += ";";
    }
    _entry.pop_back();

    // check if given proto (4th entry in line) is a valid proto
    std::string proto = values[3].GetString();
    if (valid_protos.find(proto) == valid_protos.end()) {
        DARWIN_LOG_WARNING("TAnomalyTask:: ParseLine:: proto " + proto + " is not valid");
        return false;
    }

    return true;
}

bool AnomalyTask::REDISAddEntry() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("TAnomalyTask::REDISAddEntry:: Add data in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_list_name);
    arguments.emplace_back(_entry);

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyTask::REDISAdd:: Not the expected Redis response");
        return false;
    }

    return true;
}
