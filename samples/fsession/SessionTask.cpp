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
                         std::mutex& cache_mutex,
                         std::unordered_map<std::string, std::unordered_map<std::string, id_timeout>> &applications)
        : Session{"session", socket, manager, cache, cache_mutex}, _applications{applications} {
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

            certitude = ReadFromSession();
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

bool SessionTask::ReadFromSession() noexcept {
    DARWIN_LOGGER;

    //Check that the session value has the expected length
    if (_token.size() != TOKEN_SIZE) {
        DARWIN_LOG_ERROR("SessionTask::ReadFromSession:: Invalid token size: " + std::to_string(_token.size()) +
                         ". Expected size: " + std::to_string(TOKEN_SIZE));

        return false;
    }
    // Retrieve ID and timeout associated
    // .find returns a generator
	auto it_paths = _applications.find(_domain);
	if(it_paths == _applications.end()){
		DARWIN_LOG_ERROR("SessionTask::ReadFromSession:: Cannot find application having domain='" + _domain +
				                 "' in configuration");
	} else {
		/** Master rustine by TCA !
		 * Loop over paths and find the one beginning by _path (ex: /toto/ < /toto/tutu)
		 * :return iterator
		**/
		auto it_found_path = std::find_if(it_paths->second.begin(), it_paths->second.end(),
		                                  [this](const auto& f){return this->_path.rfind(f.first,0)==0;});
		// If iterator returns empty
		if(it_found_path == it_paths->second.end()) {
			DARWIN_LOG_ERROR("SessionTask::ReadFromSession:: Cannot find application having domain='" + _domain +
			                 "', path='" + _path + "' in configuration");
		} else {
			// it_found_path->first contains the key (path), and it_found_path->second contains value (struct id_timeout)
			return REDISLookup(it_found_path->second) == 1;
		}
	}

	return false;
}

bool SessionTask::REDISResetExpire(const uint64_t expiration) {
    DARWIN_LOGGER;
    long long int ttl;
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if (redis.Query(std::vector<std::string>{"TTL", _token}, ttl, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_WARNING("SessionTask::REDISResetExpire:: Did not get the expected result from querying "
                            "Redis while getting TTL of cookie token " + _token);
        return false;
    }

    // if TTL == -2, key does not exist, -1 means it has no TTL (it shouldn't)
    if (ttl > -2 and ttl < (long long int)expiration) {
        DARWIN_LOG_DEBUG("SessionTask::REDISResetExpire:: resetting expiration to " + std::to_string(expiration));
        if (redis.Query(std::vector<std::string>{"EXPIRE", _token, std::to_string(expiration)}, true)
                != REDIS_REPLY_INTEGER) {
            DARWIN_LOG_WARNING("SessionTask::REDISResetExpire:: could not reset the expiration of cookie token "
                                + _token);
            return false;
        }
    }

    return true;
}


unsigned int SessionTask::REDISLookup(const id_timeout &id_t) noexcept {
    DARWIN_LOGGER;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    int redis_reply;
    std::vector<std::any> result_vector;

    std::string app_id = id_t.app_id;
    uint64_t expiration = id_t.timeout;

    std::string result;
    std::string key = _token + "_" + app_id;
    std::vector<std::string> arguments;
    arguments.emplace_back("GET");
    arguments.emplace_back(key);

    redis_reply = redis.Query(arguments, result, true);

    if(redis_reply == REDIS_REPLY_STRING) {
        // key exists, but still needs to check value
        if (result != "1") {
            DARWIN_LOG_INFO("SessionTask::REDISLookup:: got a valid key, but got '" + result + "' instead of '1'");
        }
        DARWIN_LOG_INFO("SessionTask::REDISLookup:: Cookie " + _token + " authenticated on repository " +
                        app_id);
        if (expiration)
            REDISResetExpire(expiration);
        return 1;

    } else if (redis_reply == REDIS_REPLY_NIL) {
        DARWIN_LOG_DEBUG("SessionTask::REDISLookup:: no result for key " + key);

    } else {
        // not the expected response
        DARWIN_LOG_WARNING("SessionTask::REDISLookup:: Something went wrong while querying Redis");
    }

    return 0;
}

bool SessionTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The input line is not an array");
        return false;
    }

    _token.clear();
    auto values = line.GetArray();

    if (values.Size() <= 0) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The list provided is empty");
        return false;
    }
    else if (values.Size() != 3) {
        DARWIN_LOG_ERROR(
                "SessionTask:: ParseBody: You must provide three arguments per request: the token, "
                "the domain and the path"
        );

        return false;
    }

    if (!values[0].IsString()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The token must be a string");
        return false;
    }

    if (!values[1].IsString()) {
        DARWIN_LOG_ERROR("SessionTask:: ParseBody: The domain must be a string");
        return false;
    }

	if (!values[2].IsString()) {
		DARWIN_LOG_ERROR("SessionTask:: ParseBody: The path must be a string");
		return false;
	}

    _token = values[0].GetString();
	_domain = values[1].GetString();
	_path = values[2].GetString();

    DARWIN_LOG_DEBUG("SessionTask:: ParseBody: Parsed request: " + _token + " | " + _domain + " | " + _path);

    return true;
}
