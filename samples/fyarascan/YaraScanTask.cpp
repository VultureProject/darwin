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
#include "Logger.hpp"
#include <boost/beast/core/detail/base64.hpp>
#include "../../toolkit/Base64.h"

YaraScanTask::YaraScanTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::shared_ptr<darwin::toolkit::YaraEngine> yaraEngine)
        : Session{"yara", socket, manager, cache},
        _yaraEngine{yaraEngine} {
    DARWIN_LOGGER;
    _is_cache = _cache != nullptr;
}

xxh::hash64_t YaraScanTask::GenerateHash() {
    return xxh::xxhash<64>(std::string(_current_chunk.begin(), _current_chunk.end()));
}

long YaraScanTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_YARA_SCAN;
}

void YaraScanTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    for (const std::vector<unsigned char> &chunk : _chunks) {
        SetStartingTime();
        _current_chunk = chunk;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                DARWIN_LOG_DEBUG("YaraScanTask:: has certitude in cache");

                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                            R"(", "filter": ")" + GetFilterName() + R"(", "certitude": )" + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("YaraScanTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = _yaraEngine->ScanData(_current_chunk);

        if(certitude < 0) {
            DARWIN_LOG_WARNING("YaraScanTask:: could not scan data, ignoring chunk");
            _certitudes.push_back(0);
            continue;
        }

        if (is_log && (certitude>=_threshold)){
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
        DARWIN_LOG_DEBUG("YaraScanTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _chunks = std::vector<std::vector<unsigned char>>();
}

void YaraScanTask::Workflow() {
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

bool YaraScanTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("YaraScanTask:: ParseBody: " + std::to_string(body.size()) + " bytes");

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("YaraScanTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("YaraScanTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {

            if(!request.IsString()) {
                DARWIN_LOG_ERROR("YaraScanTask:: ParseBody: Each entry must be a string");
                return false;
            }

            std::string data(request.GetString());
            
            std::string decoded("");
            darwin::toolkit::Base64::Decode(data, decoded);
            const unsigned char* raw_decoded = reinterpret_cast<const unsigned char *>(decoded.c_str());
            std::vector<unsigned char> chunk(raw_decoded, raw_decoded + decoded.size());
            _chunks.push_back(chunk);
        }

        DARWIN_LOG_INFO("YaraScanTask:: ParseBody: finished parsing " + std::to_string(_chunks.size()) + " input(s)");
    } catch (...) {
        DARWIN_LOG_CRITICAL("YaraScanTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
