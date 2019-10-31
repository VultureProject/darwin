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
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                         std::shared_ptr<darwin::toolkit::RedisManager> redis_manager)
        : Session{"session", socket, manager, cache},
          _redis_manager{redis_manager}{
    _is_cache = _cache != nullptr;
}

long SessionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_SESSION;
}

xxh::hash64_t SessionTask::GenerateHash() {
    return xxh::xxhash<64>(_token + "|" + boost::join(_repo_ids, "-"));
}

void SessionTask::operator()() {
    DARWIN_LOGGER;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        if(ParseLine(line)) {
            SetStartingTime();
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

            certitude = ReadFromSession(_token, _repo_ids);
            _certitudes.push_back(certitude);

            if (_is_cache) {
                SaveToCache(hash, certitude);
            }
            DARWIN_LOG_DEBUG("SessionTask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }

    Workflow();
}

void SessionTask::Workflow() {
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

    redisReply *reply = nullptr;
    unsigned int session_status = 0;

    if (repo_ids.empty()) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: No repository ID given");
        return DARWIN_ERROR_RETURN;
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

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        return DARWIN_ERROR_RETURN;
    }

    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 1) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: Bad entry retrieved with token " + token +
                         " and repository IDs " + parsed_repo_ids);

        session_status = 0;

    } else {
        session_status = 0;
        std::size_t session_status_index = 0;

        DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: " + std::to_string(reply->elements) + " element(s) retrieved");

        while (session_status_index < reply->elements) {
            if (reply->element[session_status_index]->type == REDIS_REPLY_STRING) {
                if (std::string(reply->element[session_status_index]->str) == "1") {
                    DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: Valid repository ID found");
                    session_status = 1;
                    break;
                }
            } else if (reply->element[session_status_index]->type == REDIS_REPLY_INTEGER) {
                if (reply->element[session_status_index]->integer == 1) {
                    DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: Valid repository ID found");
                    session_status = 1;
                    break;
                }
            } else {
                DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: Unexpected type found. Passing");
            }

            ++session_status_index;
        }
    }

    freeReplyObject(reply);
    reply = nullptr;

    DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " authenticated on repository IDs " +
                    parsed_repo_ids + " = " + std::to_string(session_status));

    return session_status;
}

bool SessionTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The input line is not an array");
        return false;
    }

    _token.clear();
    _repo_ids.clear();
    auto values = line.GetArray();

    if (values.Size() <= 0) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The list provided is empty");
        return false;
    }

    if (values.Size() != 2) {
        DARWIN_LOG_ERROR(
                "SessionTask:: ParseBody: You must provide exactly two arguments per request: the token and"
                " repository IDs"
        );

        return false;
    }

    if (!values[0].IsString()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The token must be a string");
        return false;
    }

    if (!values[1].IsString()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The repository IDs sent must be a string in the following "
                            "format: REPOSITORY1;REPOSITORY2;...");
        return false;
    }

    _token = values[0].GetString();
    std::string raw_repo_ids = values[1].GetString();
    boost::split(_repo_ids, raw_repo_ids, [](char c) {return c == ';';});

    DARWIN_LOG_DEBUG("SessionTask:: ParseBody: Parsed request: " + _token + " | " + raw_repo_ids);

    return true;
}
