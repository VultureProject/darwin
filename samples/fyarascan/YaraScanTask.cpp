/// \file     YaraScanTask.cpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "../toolkit/rapidjson/stringbuffer.h"
#include "../toolkit/rapidjson/writer.h"
#include "YaraScanTask.hpp"
#include "Stats.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"

#include "../../toolkit/Encoders.h"

YaraScanTask::YaraScanTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               std::shared_ptr<darwin::toolkit::YaraEngine> yaraEngine)
        : Session{"yara", socket, manager, cache, cache_mutex},
        _yaraEngine{yaraEngine} {
    DARWIN_LOGGER;
    _is_cache = _cache != nullptr;
}

xxh::hash64_t YaraScanTask::GenerateHash() {
    return xxh::xxhash<64>(_chunk);
}

long YaraScanTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_YARA_SCAN;
}

void YaraScanTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for(auto &line : array) {
        STAT_INPUT_INC;
        SetStartingTime();
        unsigned int certitude;
        xxh::hash64_t hash;

        if(ParseLine(line)) {
            if (_is_cache) {
                hash = GenerateHash();

                if (GetCacheResult(hash, certitude)) {
                    DARWIN_LOG_DEBUG("YaraScanTask:: has certitude in cache");

                    if (certitude>=_threshold and certitude < DARWIN_ERROR_RETURN){
                        STAT_MATCH_INC;
                        std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) + "}\n";
                        DARWIN_RAISE_ALERT(alert_log);
                    }
                    _certitudes.push_back(certitude);
                    DARWIN_LOG_DEBUG("YaraScanTask:: processed entry in "
                                    + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                    continue;
                }
            }

            const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(_chunk.c_str());
            std::vector<unsigned char> data(raw_decoded, raw_decoded + _chunk.size());

            certitude = _yaraEngine->ScanData(data);

            if(certitude < 0) {
                DARWIN_LOG_WARNING("YaraScanTask:: could not scan data, ignoring chunk");
                _certitudes.push_back(DARWIN_ERROR_RETURN);
                continue;
            }

            if (certitude>=_threshold){
                rapidjson::Document results = _yaraEngine->GetResults();
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                results.Accept(writer);

                _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                        R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) + 
                        R"(, "yara": )" + buffer.GetString() + "}\n";
            }
            _certitudes.push_back(certitude);

            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
        }
        else {
            STAT_PARSE_ERROR_INC;
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }

        DARWIN_LOG_DEBUG("YaraScanTask:: processed entry in "
                        + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }
}

bool YaraScanTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;
    std::string encoding;
    std::string encodedChunk;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: the input line is not an array");
        return false;
    }

    _chunk.clear();
    auto fields = line.GetArray();

    if(fields.Size() != 2) {
        DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: You must provide the encoding and the data as 2 separate string fields");
        return false;
    }

    if(not fields[0].IsString() || not fields[1].IsString()) {
        DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: fields should both be string");
        return false;
    }

    encoding = fields[0].GetString();
    encodedChunk = fields[1].GetString();
    if(encoding.compare("hex") == 0 || encoding.compare("HEX") == 0) {
        std::string err = darwin::toolkit::Hex::Decode(encodedChunk, _chunk);
        if(not err.empty()) {
            DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: error while decoding hex data -> " + err);
            return false;
        }
    }
    else if(encoding.compare("base64") == 0 || encoding.compare("BASE64") == 0) {
        std::string err = darwin::toolkit::Base64::Decode(encodedChunk, _chunk);
        if(not err.empty()) {
            DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: error while decoding base64 data -> " + err);
            return false;
        }
    }
    else {
        DARWIN_LOG_ERROR("YaraScanTask:: ParseLine: unsuported encoding -> " + encoding +
                        ", supported encodings are base64/BASE64 and hex/HEX");
        return false;
    }

    DARWIN_LOG_DEBUG("YaraScanTask:: ParseLine: line parse successful (got " + std::to_string(_chunk.size()) + " bytes)");
}
