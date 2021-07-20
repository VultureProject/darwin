/// \file     YaraTask.cpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>
#include <boost/algorithm/string.hpp>

#include "YaraTask.hpp"
#include "ASession.hpp"
#include "Stats.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"


YaraTask::YaraTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               darwin::session_ptr_t s,
                               darwin::DarwinPacket& packet,
                               std::shared_ptr<darwin::toolkit::YaraEngine> yaraEngine)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet),
        _yaraEngine{yaraEngine} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t YaraTask::GenerateHash() {
    return xxh::xxhash<64>(_chunk);
}

long YaraTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_YARA_SCAN;
}

void YaraTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = _s->GetOutputType() == darwin::config::output_type::LOG;
    auto logs = _packet.GetMutableLogs();

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for(auto &line : array) {
        STAT_INPUT_INC;
        SetStartingTime();
        unsigned int certitude = 0;
        xxh::hash64_t hash;

        if(ParseLine(line)) {
            if (_is_cache) {
                hash = GenerateHash();

                if (GetCacheResult(hash, certitude)) {
                    DARWIN_LOG_DEBUG("YaraTask:: has certitude in cache");

                    if (certitude>=_threshold and certitude < DARWIN_ERROR_RETURN){
                        STAT_MATCH_INC;

                        DARWIN_ALERT_MANAGER.Alert("raw_data", certitude, _s->Evt_idToString());

                        if (is_log) {
                            std::string alert_log = R"({"evt_id": ")" + _s->Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) +
                                "}\n";
                            logs += alert_log + '\n';
                        }
                    }
                    _packet.AddCertitude(certitude);
                    DARWIN_LOG_DEBUG("YaraTask:: processed entry in "
                                    + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                    continue;
                }
            }

            const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(_chunk.c_str());
            std::vector<unsigned char> data(raw_decoded, raw_decoded + _chunk.size());

            if(_yaraEngine->ScanData(data, certitude) == -1) {
                DARWIN_LOG_WARNING("YaraTask:: error while scanning, ignoring chunk");
                _packet.AddCertitude(DARWIN_ERROR_RETURN);
                continue;
            }

            if (certitude >= _threshold){
                STAT_MATCH_INC;
                darwin::toolkit::YaraResults results = _yaraEngine->GetResults();

                std::string ruleListJson = YaraTask::GetJsonListFromSet(results.rules);
                std::string tagListJson = YaraTask::GetJsonListFromSet(results.tags);
                std::string details = "{\"rules\": " + ruleListJson + "}";

                DARWIN_ALERT_MANAGER.Alert("raw_data", certitude, _s->Evt_idToString(),  details, tagListJson);

                if (is_log) {
                    std::string alert_log = R"({"evt_id": ")" + _s->Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                            R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) +
                            R"(, "rules": )" + ruleListJson + R"(, "tags": )" + tagListJson + "}\n";
                    logs += alert_log + '\n';
                }
            }
            _packet.AddCertitude(certitude);

            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
        }
        else {
            STAT_PARSE_ERROR_INC;
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }

        DARWIN_LOG_DEBUG("YaraTask:: processed entry in "
                        + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(_packet.GetCertitudeList().back()));
    }
}

bool YaraTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;
    std::string encoding;
    std::string encodedChunk;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("YaraTask:: ParseLine: the input line is not an array");
        return false;
    }

    _chunk.clear();
    auto fields = line.GetArray();

    switch(fields.Size()){
        case 2:
            if(not fields[1].IsString()){
                DARWIN_LOG_ERROR("YaraTask:: ParseLine: second field should be a string");
                return false;
            }
            else {
                encoding = fields[1].GetString();
            }
            // No break here!
        case 1:
            if(not fields[0].IsString()){
                DARWIN_LOG_ERROR("YaraTask:: ParseLine: first field should be a string");
                return false;
            }
            else {
                encodedChunk = fields[0].GetString();
            }
            break;
        default:
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: This filter accepts between 1 and 2 parameters (chunk [encoding])");
            return false;
    }

    if(encoding.empty()) {
        _chunk = encodedChunk;
    }
    else if(boost::iequals(encoding, "hex")) {
        std::string err = darwin::toolkit::Hex::Decode(encodedChunk, _chunk);
        if(not err.empty()) {
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: error while decoding hex data -> " + err);
            return false;
        }
    }
    else if(boost::iequals(encoding, "base64")) {
        std::string err = darwin::toolkit::Base64::Decode(encodedChunk, _chunk);
        if(not err.empty()) {
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: error while decoding base64 data -> " + err);
            return false;
        }
    }
    else {
        DARWIN_LOG_ERROR("YaraTask:: ParseLine: unsuported encoding -> " + encoding +
                        ", supported encodings are base64 and hex");
        return false;
    }

    DARWIN_LOG_DEBUG("YaraTask:: ParseLine: line parse successful (got " + std::to_string(_chunk.size()) + " bytes)");
    return true;
}

std::string YaraTask::GetJsonListFromSet(std::set<std::string> &input) {
    std::string result;
    // Reserve some space for each entry (10 chars) + 2 for beginning and end of list
    // This will be too much almost every time, but this doesn't matter as the string will be short-lived
    // and the input won't contain more than a dozen fields
    result.reserve(input.size() * 32 + 2);

    result = "[";
    bool first = true;
    for(auto rule : input) {
        if(not first)
            result += ",";
        result += "\"" + rule + "\"";
        first = false;
    }
    result += "]";

    return result;
}