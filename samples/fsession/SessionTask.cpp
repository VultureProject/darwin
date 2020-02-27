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
#include "protocol.h"


SessionTask::SessionTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                         std::mutex& cache_mutex)
        : Session{"session", socket, manager, cache, cache_mutex}{
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
            _certitudes.push_back(certitude);

            DARWIN_LOG_DEBUG("SessionTask:: processed entry in "
                            + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
        }
        else {
            STAT_PARSE_ERROR_INC;
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }
}

unsigned int SessionTask::ReadFromSession(const std::string &token, const std::vector<std::string> &repo_ids) noexcept {
    DARWIN_LOGGER;

    //Check that the session value has the expected length
    if (token.size() != TOKEN_SIZE) {
        DARWIN_LOG_ERROR("SessionTask::ReadFromSession:: Invalid token size: " + std::to_string(token.size()) +
                         ". Expected size: " + std::to_string(TOKEN_SIZE));

        return IS_NOT_AUTHENT;
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
    bool found = false;

    if (repo_ids.empty()) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: No repository ID given");
        return DARWIN_ERROR_RETURN;
    }

    std::vector<std::string> arguments;
    arguments.emplace_back("HMGET");
    arguments.emplace_back(token);
    arguments.insert(arguments.end(), repo_ids.begin(), repo_ids.end());
    /*
        KEY                 HMAP
        sess_<SHA256>       login, authenticated
    */

   if(redis.Query(arguments, result) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("SessionTask::REDISLookup:: Something went wrong while querying Redis");
        return DARWIN_ERROR_RETURN;
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
        found = false;

        try {
            std::string replyObject(std::any_cast<std::string>(object));
            if(replyObject == "1") {
                session_status = IS_AUTHENT;
                found = true;
            }else{
                DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " not authenticated on repository IDs " +
                    JoinRepoIDs(repo_ids) + " = " + std::to_string(IS_NOT_AUTHENT));
                return IS_NOT_AUTHENT;
            }
        }
        catch(const std::bad_any_cast&) {}

        if(not found){
            try {
                int replyObject = std::any_cast<int>(object);
                if(replyObject == 1) {
                    session_status = IS_AUTHENT;
                }else{
                    DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " not authenticated on repository IDs " +
                        JoinRepoIDs(repo_ids) + " = " + std::to_string(IS_NOT_AUTHENT));
                    return IS_NOT_AUTHENT;
                }
            }
            catch(std::bad_any_cast&) {
                DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " not authenticated on repository IDs " +
                    JoinRepoIDs(repo_ids) + " = " + std::to_string(IS_NOT_AUTHENT));
                return IS_NOT_AUTHENT;
            }
        }
        
    }

    DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie given " + token + " authenticated on repository IDs " +
                    JoinRepoIDs(repo_ids) + " = " + std::to_string(session_status));

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
    boost::split(_repo_ids, raw_repo_ids, [](char c) {return c == ';';}, boost::token_compress_on);
    if(_repo_ids.at(0) == ""){
        _repo_ids.erase(_repo_ids.begin());
    }
    if(_repo_ids.at(_repo_ids.size()-1) == ""){
        _repo_ids.erase(_repo_ids.end());
    }

    DARWIN_LOG_DEBUG("SessionTask:: ParseBody: Parsed request: " + _token + " | " + raw_repo_ids);

    return true;
}
