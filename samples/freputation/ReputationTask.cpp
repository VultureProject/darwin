/// \file     ReputationTask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     10/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Logger.hpp"
#include "Network.hpp"
#include "protocol.h"
#include "ReputationTask.hpp"


ReputationTask::ReputationTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               MMDB_s* db)
        : Session{"reputation", socket, manager, cache}, _database{db} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t ReputationTask::GenerateHash() {
    return xxh::xxhash<64>(_current_ip_address + "|" + boost::join(_current_tags, "-"));
}

long ReputationTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_REPUTATION;
}

void ReputationTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // We have a generic hash function, which takes no arguments as these can be of very different types depending
    // on the nature of the filter
    // So instead, we set an attribute corresponding to the current IP address and tags being processed, to compute the
    // hash accordingly
    for (std::size_t index = 0; index < _ip_addresses.size(); ++index) {
        SetStartingTime();
        _current_ip_address = _ip_addresses[index];
        _current_tags = _tags_list[index];
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                if (is_log && (certitude>=_threshold)){
                    bool tag_found = false;
                    _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                            R"(", "filter": ")" + GetFilterName() + R"(", "ip" :")" + _current_ip_address + R"(", "tags": [)";

                    for (auto const& tag: _current_tags) {
                        _logs += "\"" + tag + "\",";
                        tag_found = true;
                    }

                    if (tag_found){
                        _logs.pop_back();
                    }

                    _logs += "], \"certitude\": " + std::to_string(certitude) + "}\n";
                }
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("ReputationTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = GetReputation(_current_ip_address);
        if (is_log && (certitude>=_threshold)){
            bool tag_found = false;
            _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
                            R"(", "filter": ")" + GetFilterName() + R"(", "ip": ")" + _current_ip_address + R"(", "tags": [)";

            for (auto const& tag: _current_tags) {
                _logs += "\"" + tag + "\",";
                tag_found = true;
            }

            if (tag_found){
                _logs.pop_back();
            }

            _logs += "], \"certitude\": " + std::to_string(certitude) + "}\n";
        }
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("ReputationTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _ip_addresses = std::vector<std::string>();
    _tags_list = std::vector<std::unordered_set<std::string>>();
}

void ReputationTask::Workflow(){
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

bool ReputationTask::ReadFromSession(const std::string &ip_address,
                                     MMDB_lookup_result_s &result) noexcept {
    DARWIN_LOGGER;
    int ip_type = -1;

    darwin::network::GetIpAddressType(ip_address, &ip_type);

    switch (ip_type) {
        case AF_INET:
            return DBLookupIn(ip_address, result);
        case AF_INET6:
            return DBLookupIn6(ip_address, result);
        default:
            DARWIN_LOG_ERROR("ReputationTask:: Data type not handled");
            return false;
    }
}

bool ReputationTask::DBLookupIn(const std::string &ip_address, MMDB_lookup_result_s &result) noexcept {
    DARWIN_LOGGER;
    int mmdb_error = 0;
    sockaddr_in saddr;

    darwin::network::GetSockAddrIn(ip_address, &saddr);

    result = MMDB_lookup_sockaddr(_database, (struct sockaddr*) &saddr,
                                  &mmdb_error);

    if (mmdb_error != MMDB_SUCCESS) {
        DARWIN_LOG_ERROR("ReputationTask:: Error reading data from DB");
        return false;
    }
    return true;
}

bool ReputationTask::DBLookupIn6(const std::string &ip_address, MMDB_lookup_result_s &result) noexcept {
    DARWIN_LOGGER;
    int mmdb_error = 0;
    sockaddr_in6 saddr;

    darwin::network::GetSockAddrIn6(ip_address, &saddr);

    result = MMDB_lookup_sockaddr(_database, (struct sockaddr*) &saddr,
                                  &mmdb_error);

    if (mmdb_error != MMDB_SUCCESS) {
        DARWIN_LOG_ERROR("ReputationTask:: Error reading data from DB");
        return false;
    }

    return true;
}

unsigned int ReputationTask::GetReputation(const std::string &ip_address) noexcept {
    DARWIN_LOGGER;
    MMDB_lookup_result_s result;
    MMDB_entry_data_s entry_data;

    if (!ReadFromSession(ip_address, result)) return 101;

    DARWIN_LOG_DEBUG("ReputationTask:: Searching reputation");
    if (!result.found_entry) {
        DARWIN_LOG_DEBUG("ReputationTask:: IP not found in DB");
        return 0;
    }

    auto status = MMDB_get_value(&result.entry, &entry_data,
                                 "reputation", NULL);
    if (status != MMDB_SUCCESS) {
        DARWIN_LOG_ERROR("ReputationTask:: Error reading data from DB results");
        return 100; // We block, by default
    }

    if (!entry_data.has_data) {
        DARWIN_LOG_ERROR("ReputationTask:: Reputation not found in DB results");
        return 100; // We block, by default
    }

    std::string tag;
    tag.assign(entry_data.utf8_string, entry_data.data_size);
    DARWIN_LOG_DEBUG(std::string("ReputationTask:: Reputation tag: ") + tag);
    bool is_malicious = _current_tags.find(tag) != _current_tags.end(); // The tag found matches our list... or not
    DARWIN_LOG_DEBUG(_current_ip_address + " in database with a matching tag: " + (is_malicious ? "true" : "false"));
    return is_malicious ? 100 : 0;
}

bool ReputationTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ReputationTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("ReputationTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("ReputationTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("ReputationTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 2) {
                DARWIN_LOG_ERROR(
                        "ReputationTask:: ParseBody: You must provide exactly two arguments per request: the IP address and"
                        " the tags"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("ReputationTask:: ParseBody: The IP address must be a string");
                return false;
            }

            std::string ip_address = items[0].GetString();

            if (!items[1].IsString()) {
                DARWIN_LOG_ERROR("ReputationTask:: ParseBody: The tags sent must be a string in the following format: "
                                 "TAG1;TAG2;...");
                return false;
            }

            _ip_addresses.push_back(ip_address);
            std::unordered_set<std::string> tags;
            std::string raw_tags = items[1].GetString();
            boost::split(tags, raw_tags, [](char c) {return c == ';';});
            _tags_list.push_back(tags);

            DARWIN_LOG_DEBUG("ReputationTask:: Parsed body: " + ip_address + ", " + raw_tags);

        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("ReputationTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
