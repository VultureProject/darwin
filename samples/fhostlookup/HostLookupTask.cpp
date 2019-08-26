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
        : Session{socket, manager, cache}, _database{db} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t HostLookupTask::GenerateHash() {
    return xxh::xxhash<64>(_current_host);
}

long HostLookupTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_HOSTLOOKUP;
}

void HostLookupTask::operator()() {
    DARWIN_ACCESS_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // We have a generic hash function, which takes no arguments as these can be of very different types depending
    // on the nature of the filter
    // So instead, we set an attribute corresponding to the current host being processed, to compute the hash
    // accordingly
    for (const std::string &host : _hosts) {
        SetStartingTime();
        _current_host = host;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                if (is_log && (certitude>=_threshold)){
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + R"(", "host": ")" + host +
                             R"(", "certitude": )" + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                DARWIN_LOG_ACCESS(_current_host.size(), certitude, GetDuration());
                continue;
            }
        }

        certitude = DBLookup(host);
        if (is_log && (certitude>=_threshold)){
            _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + GetTime() + R"(", "host": ")" + host +
                     R"(", "certitude": )" + std::to_string(certitude) + "}\n";
        }
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }

        DARWIN_LOG_ACCESS(_current_host.size(), certitude, GetDuration());
    }

    Workflow();
    _hosts = std::vector<std::string>();
}

std::string HostLookupTask::GetTime(){
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

void HostLookupTask::Workflow() {
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

unsigned int HostLookupTask::DBLookup(const std::string &host) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookupTask:: Looking up '" +  host + "' in the database");
    unsigned int certitude = 0;

    if(_database.find(host) != _database.end()) {
        certitude = 100;
    }

    DARWIN_LOG_DEBUG("Reputation is " + std::to_string(certitude));
    return certitude;
}

bool HostLookupTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookupTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                        "HostLookupTask:: ParseBody: You must provide exactly one argument per request: the host"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("HostLookupTask:: ParseBody: The host sent must be a string");
                return false;
            }

            std::string host = items[0].GetString();
            _hosts.push_back(host);
            DARWIN_LOG_DEBUG("HostLookupTask:: ParseBody: Parsed element: " + host);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("HostLookupTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
