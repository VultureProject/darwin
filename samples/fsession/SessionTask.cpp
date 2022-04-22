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
#include "Stats.hpp"
#include "SessionTask.hpp"
#include "ASession.hpp"


SessionTask::SessionTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                        std::mutex& cache_mutex,
                        darwin::session_ptr_t s,
                        darwin::DarwinPacket& packet)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet){
}

long SessionTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_SESSION;
}

void SessionTask::operator()() {
    DARWIN_LOGGER;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        STAT_INPUT_INC;
        if(ParseLine(line)) {
            SetStartingTime();
            unsigned int certitude;

            certitude = ReadFromSession(_token, _repo_ids);
            if(certitude)
                STAT_MATCH_INC;
            _packet.AddCertitude(certitude);

            DARWIN_LOG_DEBUG("SessionTask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            STAT_PARSE_ERROR_INC;
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }
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

    return REDISLookup(token, repo_ids) == 1;
}

std::string SessionTask::JoinRepoIDs(const std::vector<std::string> &repo_ids) {
    std::string parsed_repo_ids;

    for (auto it = repo_ids.begin(); it != repo_ids.end(); ++it) {
        parsed_repo_ids += *it;

        if (it != repo_ids.end() - 1) parsed_repo_ids += " ";
    }

    return parsed_repo_ids;
}


bool SessionTask::REDISResetExpire(const std::string &token, const std::string &repo_id __attribute((unused))) {
    DARWIN_LOGGER;
    long long int ttl;
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if (redis.Query(std::vector<std::string>{"TTL", token}, ttl, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("SessionTask::REDISResetExpire:: Did not get the expected result from querying "
                            "Redis while getting TTL of cookie token " + token);
        return false;
    }

    // if TTL == -2, key does not exist, -1 means it has no TTL (it shouldn't)
    if (ttl > -2 and ttl < (long long int)_expiration) {
        DARWIN_LOG_DEBUG("SessionTask::REDISResetExpire:: resetting expiration to " + std::to_string(_expiration));
        if (redis.Query(std::vector<std::string>{"EXPIRE", token, std::to_string(_expiration)}, true)
                != REDIS_REPLY_INTEGER) {
            DARWIN_LOG_WARNING("SessionTask::REDISResetExpire:: could not reset the expiration of cookie token "
                                + token);
            return false;
        }
    }

    return true;
}


unsigned int SessionTask::REDISLookup(const std::string &token, const std::vector<std::string> &repo_ids) noexcept {
    DARWIN_LOGGER;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    int redis_reply;
    std::vector<std::any> result_vector;

    if (repo_ids.empty()) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: No repository ID given");
        return DARWIN_ERROR_RETURN;
    }

    for (auto &repo_id : repo_ids) {
        std::string result;
        std::string key = token + "_" + repo_id;
        std::vector<std::string> arguments;
        arguments.emplace_back("GET");
        arguments.emplace_back(key);

        redis_reply = redis.Query(arguments, result, true);

        if(redis_reply == REDIS_REPLY_STRING) {
            // key exists, but still needs to check value
            if (result != "1") {
                DARWIN_LOG_INFO("SessionTask::REDISLookup:: got a valid key, but got '" + result + "' instead of '1'");
                continue;
            }
            DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie " + token + " authenticated on repository " +
                            repo_id);
            if (_expiration)
                REDISResetExpire(token, repo_id);
            return 1;
        } else if (redis_reply == REDIS_REPLY_NIL) {
            DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: no result for key " + key);
            // key does not exist
            continue;
        } else {
            // not the expected response
            DARWIN_LOG_WARNING("SessionTask::REDISLookup:: Something went wrong while querying Redis");
            continue;
        }
    }


    return 0;
}

bool SessionTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;
    _expiration = 0;

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
    else if (values.Size() < 2) {
        DARWIN_LOG_ERROR(
                "SessionTask:: ParseBody: You must provide at least two arguments per request: the token and"
                " repository ID"
        );

        return false;
    }
    else if (values.Size() > 3) {
        DARWIN_LOG_ERROR(
                "SessionTask:: ParseBody: You must provide at most three arguments per request: the token, "
                "the repository ID and the expiration value to set to the token key"
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

    if (values.Size() == 3) {
        if (values[2].IsString()) {
            errno = 0;
            _expiration = std::strtoll(values[2].GetString(), NULL, 10);
            if (errno) {
                DARWIN_LOG_WARNING("SessionTask:: ParseBody: expiration's value out of bounds'");
                return false;
            }
        }
        else if (values[2].IsUint64()){
            _expiration = values[2].GetUint64();
        }
        else {
            DARWIN_LOG_WARNING("SessionTask:: ParseBody: expiration should be a valid positive number");
            return false;
        }
    }

    _token = values[0].GetString();
    std::string raw_repo_ids = values[1].GetString();
    boost::split(_repo_ids, raw_repo_ids, [](char c) {return c == ';';});

    DARWIN_LOG_DEBUG("SessionTask:: ParseBody: Parsed request: " + _token + " | " + raw_repo_ids);

    return true;
}
