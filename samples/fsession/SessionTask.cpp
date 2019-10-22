/// \file     SessionTask.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

extern "C" {
#include <hiredis/hiredis.h>
}

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <stdexcept>
#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "Logger.hpp"
#include "SessionTask.hpp"
#include "protocol.h"


SessionTask::SessionTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache)
        : Session{"session", socket, manager, cache}{
    _is_cache = _cache != nullptr;
}

long SessionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_SESSION;
}

xxh::hash64_t SessionTask::GenerateHash() {
    return xxh::xxhash<64>(_current_token + "|" + boost::join(_current_repo_ids, "-"));
}

void SessionTask::operator()() {
    DARWIN_LOGGER;

    for (std::size_t index = 0; index < _tokens.size(); ++index) {
        SetStartingTime();
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current token and repository IDs being processed, to
        // compute the hash accordingly
        _current_token = _tokens[index];
        _current_repo_ids = _repo_ids_list[index];
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("SessionTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = ReadFromSession(_current_token, _current_repo_ids);
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("SessionTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _tokens = std::vector<std::string>();
    _repo_ids_list = std::vector<std::vector<std::string>>();
}

void SessionTask::Workflow() {
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

bool SessionTask::ReadFromSession(const std::string &token, const std::vector<std::string> &repo_ids) noexcept {
    DARWIN_LOGGER;

    //Check that the session value has the expected length
    if (token.size() != TOKEN_SIZE) {
        DARWIN_LOG_ERROR("SessionTask::ReadFromSession:: Invalid token size: " + std::to_string(token.size()) +
                         ". Expected size: " + std::to_string(TOKEN_SIZE));

        return false;
    }

    return REDISLookup(token, repo_ids);
}

std::string SessionTask::JoinRepoIDs(const std::vector<std::string> &repo_ids) {
    std::string parsed_repo_ids;

    for (auto it = repo_ids.begin(); it != repo_ids.end(); ++it) {
        parsed_repo_ids += *it;

        if (it != repo_ids.end() - 1) parsed_repo_ids += " ";
    }

    return parsed_repo_ids;
}

unsigned int SessionTask::REDISLookup(const std::string &token, const std::vector<std::string> &repo_ids) noexcept {
    DARWIN_LOGGER;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    unsigned int session_status = 0;
    std::any result;
    std::vector<std::any> result_vector;

    if (repo_ids.empty()) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: No repository ID given");
        return 101;
    }

    std::vector<std::string> arguments;
    arguments.emplace_back("HMGET");
    arguments.emplace_back(token);
    arguments.insert(arguments.end(), repo_ids.begin(), repo_ids.end());
    std::string parsed_repo_ids = JoinRepoIDs(repo_ids);
    /*
        KEY                 HMAP
        sess_<SHA256>       login, authenticated
    */

   if(redis.Query(arguments, result) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: Something went wrong while querying Redis");
        return 0;
    }

    try {
        result_vector = std::any_cast<std::vector<std::any>>(result);
        DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: " + std::to_string(result_vector.size()) + " element(s) retrieved");
    }
    catch(const std::bad_any_cast& error) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: wrong answer format: " + std::string(error.what()));
        return 0;
    }

    for(auto& object : result_vector) {
        try {
            std::string replyObject(std::any_cast<std::string>(object));
            if(replyObject == "1") {
                session_status = 1;
                break;
            }
        }
        catch(const std::bad_any_cast&) {}

        try {
            int replyObject = std::any_cast<int>(object);
            if(replyObject == 1) {
                session_status = 1;
                break;
            }
        }
        catch(std::bad_any_cast&) {}
    }

    DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " authenticated on repository IDs " +
                    parsed_repo_ids + " = " + std::to_string(session_status));

    return session_status;
}

bool SessionTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("SessionTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("SessionTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("SessionTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("SessionTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 2) {
                DARWIN_LOG_ERROR(
                        "SessionTask:: ParseBody: You must provide exactly two arguments per request: the token and"
                        " repository IDs"
                );

                return false;
            }

            if (!items[0].IsString()) {
                DARWIN_LOG_ERROR("SessionTask:: ParseBody: The token must be a string");
                return false;
            }

            std::string token = items[0].GetString();

            if (!items[1].IsString()) {
                DARWIN_LOG_ERROR("SessionTask:: ParseBody: The repository IDs sent must be a string in the following "
                                 "format: REPOSITORY1;REPOSITORY2;...");
                return false;
            }

            std::vector<std::string> repo_ids;
            std::string raw_repo_ids = items[1].GetString();
            boost::split(repo_ids, raw_repo_ids, [](char c) {return c == ';';});

            _tokens.push_back(token);
            _repo_ids_list.push_back(repo_ids);

            DARWIN_LOG_DEBUG("SessionTask:: ParseBody: Parsed request: " + token + " | " + raw_repo_ids);

        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("SessionTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
