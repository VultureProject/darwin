/// \file     HostLookup.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "HostLookupTask.hpp"
#include "Logger.hpp"
#include "protocol.h"

HostLookupTask::HostLookupTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               tsl::hopscotch_map<std::string, int>& db)
        : Session{"host_lookup", socket, manager, cache}, _database{db} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t HostLookupTask::GenerateHash() {
    return xxh::xxhash<64>(_host);
}

long HostLookupTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_HOSTLOOKUP;
}

void HostLookupTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        SetStartingTime();
        xxh::hash64_t hash;
        unsigned int certitude;

        if(ParseLine(line)) {
            if (_is_cache) {
                hash = GenerateHash();

                if (GetCacheResult(hash, certitude)) {
                    if (is_log && (certitude>=_threshold)){
                        _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                                R"(", "filter": ")" + GetFilterName() + R"(", "host": ")" + _host + R"(", "certitude": )" + std::to_string(certitude) + "}\n";
                    }
                    _certitudes.push_back(certitude);
                    DARWIN_LOG_DEBUG("HostLookupTask:: processed entry in "
                                    + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                    continue;
                }
            }

            certitude = DBLookup();
            if (is_log && (certitude>=_threshold)){
                _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                        R"(", "filter": ")" + GetFilterName() + R"(", "host": ")" + _host + R"(", "certitude": )" + std::to_string(certitude) + "}\n";
            }
            _certitudes.push_back(certitude);

            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }

        DARWIN_LOG_DEBUG("HostLookupTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(_certitudes.back()));
    }

    Workflow();
}

void HostLookupTask::Workflow() {
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

unsigned int HostLookupTask::DBLookup() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookupTask:: Looking up '" +  _host + "' in the database");
    unsigned int certitude = 0;

    if(_database.find(_host) != _database.end()) {
        certitude = 100;
    }

    DARWIN_LOG_DEBUG("Reputation is " + std::to_string(certitude));
    return certitude;
}

bool HostLookupTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: The input line is not an array");
        return false;
    }

    _host.clear();
    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: You must provide only the host in the list");
        return false;
    }

    if (!values[0].IsString()) {
        DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: The host sent must be a string");
        return false;
    }

    _host = values[0].GetString();
    DARWIN_LOG_DEBUG("HostLookupTask:: ParseBody: Parsed element: " + _host);


    return true;
}
