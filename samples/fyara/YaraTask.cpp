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

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "../toolkit/rapidjson/stringbuffer.h"
#include "../toolkit/rapidjson/writer.h"
#include "YaraTask.hpp"
#include "Stats.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"

#include "../../toolkit/Encoders.h"

YaraTask::YaraTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               std::shared_ptr<darwin::toolkit::YaraEngine> yaraEngine)
        : Session{"yara", socket, manager, cache, cache_mutex},
        _yaraEngine{yaraEngine} {
    DARWIN_LOGGER;
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
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for(auto &line : array) {
        STAT_INPUT_INC;
        SetStartingTime();
        unsigned int score;
        xxh::hash64_t hash;

        if(ParseLine(line)) {
            if (_is_cache) {
                hash = GenerateHash();

                if (GetCacheResult(hash, score)) {
                    DARWIN_LOG_DEBUG("YaraTask:: has certitude in cache");

                    if (score>=_threshold and score < DARWIN_ERROR_RETURN){
                        STAT_MATCH_INC;
                        std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(score) + "}\n";
                        DARWIN_RAISE_ALERT(alert_log);
                    }
                    _certitudes.push_back(score);
                    DARWIN_LOG_DEBUG("YaraTask:: processed entry in "
                                    + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(score));
                    continue;
                }
            }

            const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(_chunk.c_str());
            std::vector<unsigned char> data(raw_decoded, raw_decoded + _chunk.size());

            score = _yaraEngine->ScanData(data);

            if(score < 0) {
                DARWIN_LOG_WARNING("YaraTask:: could not scan data, ignoring chunk");
                _certitudes.push_back(DARWIN_ERROR_RETURN);
                continue;
            }

            if (score>=_threshold){
                rapidjson::Document results = _yaraEngine->GetResults();
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                results.Accept(writer);

                std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                        R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(score) + 
                        R"(, "yara": )" + buffer.GetString() + "}\n";
                DARWIN_RAISE_ALERT(alert_log);

                if(is_log) {
                    _logs += alert_log + "\n";
                }
                
            }
            _certitudes.push_back(score);

            if (_is_cache) {
                SaveToCache(hash, score);
            }
        }
        else {
            STAT_PARSE_ERROR_INC;
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }

        DARWIN_LOG_DEBUG("YaraTask:: processed entry in "
                        + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(_certitudes.back()));
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

    size_t num_fields = fields.Size();

    // Simple entry, no encoding
    if(num_fields == 1) {
        DARWIN_LOG_DEBUG("YaraTask:: ParseLine: no encoding given, expecting simple text");
        
        if(not fields[0].IsString()) {
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: field should be a string");
            return false;
        }

        _chunk = fields[0].GetString();
    }
    // Entry with encoding ["encoding", "encoded_chunk"]
    else if(num_fields == 2) {
        if(not fields[0].IsString() || not fields[1].IsString()) {
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: fields should both be strings");
            return false;
        }

        encoding = fields[0].GetString();
        encodedChunk = fields[1].GetString();
        if(boost::iequals(encoding, "hex")) {
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
            DARWIN_LOG_ERROR("YaraTask:: ParseLine: unsuported encoding '" + encoding +
                            "', supported encodings are base64 and hex");
            return false;
        }
    }
    else {
        DARWIN_LOG_ERROR("Yaratask:: ParseLine: incorrect number of fields, got " + std::to_string(num_fields) + ", but expected 1 or 2");
        return false;
    }

    DARWIN_LOG_DEBUG("YaraTask:: ParseLine: line parse successful (got " + std::to_string(_chunk.size()) + " bytes)");
    return true;
}
