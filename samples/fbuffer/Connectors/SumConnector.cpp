/// \file     SumConnector.cpp
/// \authors  Theo Bertin
/// \version  1.0
/// \date     15/12/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <algorithm>    //std::min

#include "../../../toolkit/RedisManager.hpp"

#include "SumConnector.hpp"
#include "Time.hpp"

SumConnector::SumConnector(boost::asio::io_context &context, std::string &filter_socket_path, unsigned int interval, std::vector<std::pair<std::string, std::string>> &redis_lists, unsigned int minLogLen) :
                    AConnector(context, darwin::SUM, filter_socket_path, interval, redis_lists, minLogLen) {}


bool SumConnector::ParseInputForRedis(std::map<std::string, std::string> &input_line) {
    std::string entry;

    std::string source = this->GetSource(input_line);

    if (not this->ParseData(input_line, "decimal", entry))
        return false;

    for (const auto &redis_config : this->_redis_lists) {
        // If the source in the input is equal to the source in the redis list, or the redis list's source is empty
        if (not redis_config.first.compare(source) or redis_config.first.empty())
                this->REDISAddEntry(entry, redis_config.second);
    }
    return true;
}


bool SumConnector::PrepareKeysInRedis(){
    DARWIN_LOGGER;
    std::string redis_list, reply;
    bool ret = true;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    for(auto key : this->_redis_lists) {
        redis_list = key.second;
        DARWIN_LOG_DEBUG("SumConnector::PrepareKeysInRedis:: testing key " + redis_list);
        if(redis.Query(std::vector<std::string>{"TYPE", redis_list}, reply, true) != REDIS_REPLY_STATUS) {
            DARWIN_LOG_ERROR("SumConnector::PrepareKeysInRedis:: Could not get current type of key");
            ret = false;
            continue;
        }

        DARWIN_LOG_DEBUG("SumConnector::PrepareKeysInRedis:: key '"+ redis_list + "' is '" + reply + "'");

        if(reply != "none") {
            if(reply != "string") {
                DARWIN_LOG_ERROR("SumConnector::PrepareKeysInRedis:: key '" + redis_list + "' is already present but seems"
                                " to be used by something else, cannot start the filter "
                                "and risk overriding data in Redis");
                ret = false;
                continue;
            }

            DARWIN_LOG_WARNING("SumConnector::PrepareKeysInRedis:: The key '" + redis_list + "' was already set in Redis, "
                                "the key will be overrode!");

            DARWIN_LOG_DEBUG("SumConnector::PrepareKeysInRedis:: reseting Redis key '" + redis_list + "'");
            if(redis.Query(std::vector<std::string>{"DEL", redis_list}) != REDIS_REPLY_INTEGER) {
                DARWIN_LOG_WARNING("SumConnector::PrepareKeysInRedis:: could not reset the key");
            }
        }
    }

    return ret;
}


bool SumConnector::REDISAddEntry(const std::string &entry, const std::string &sum_name) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("SumConnector::REDISAddEntry:: Incrementing sum in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.clear();
    arguments.emplace_back("INCRBYFLOAT");
    arguments.emplace_back(sum_name);
    arguments.emplace_back(entry);

    if(redis.Query(arguments, true) != REDIS_REPLY_STRING) {
        DARWIN_LOG_ERROR("SumConnector::REDISAddEntry:: Not the expected Redis response, impossible to increment redis key " + sum_name);
        DARWIN_LOG_DEBUG("SumConnector::REDISAddEntry:: entry was " + entry);
        return false;
    }
    return true;
}


bool SumConnector::REDISReinsertLogs(std::vector<std::string> &logs __attribute__((unused)), const std::string &sum_name __attribute__((unused))) {
    DARWIN_LOGGER;
    DARWIN_LOG_WARNING("SumConnector::REDISReinsertLogs:: Will not reinsert logs, data lost");
    return true;
}


bool SumConnector::REDISPopLogs(long long int len __attribute__((unused)), std::vector<std::string> &logs, const std::string &sum_name) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("SumConnector::REDISPopLogs:: Querying Redis for sum key...");

    int redis_reply;
    std::any result;
    std::string result_string;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    redis_reply = redis.Query(std::vector<std::string>{"GETSET", sum_name, "0"}, result, true);
    if (redis_reply == REDIS_REPLY_NIL) {
        DARWIN_LOG_DEBUG("SumConnector:: REDISPopLogs:: key '" + sum_name + "' did not exist");
        result = std::string("0");
    }
    else if(redis_reply != REDIS_REPLY_STRING) {
        DARWIN_LOG_ERROR("SumConnector::REDISPopLogs:: Not the expected Redis response");
        return false;
    }

    try {
        result_string = std::any_cast<std::string>(result);
    }
    catch (const std::bad_any_cast&) {
        DARWIN_LOG_ERROR("SumConnector:REDISPopLogs:: Impossible to cast redis response into a string.");
        return false;
    }

    DARWIN_LOG_DEBUG("SumConnector::REDISPopLogs:: Got '" + result_string + "' from Redis");

    logs.emplace_back(result_string);

    return true;
}


long long int SumConnector::REDISListLen(const std::string &sum_name) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("SumConnector::REDISListLen:: Querying Redis for sum size...");

    std::string result_string;
    long double result = 0.0L;
    int redis_reply;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    redis_reply = redis.Query(std::vector<std::string>{"GET", sum_name}, result_string, true);
    if (redis_reply == REDIS_REPLY_NIL) {
        DARWIN_LOG_DEBUG("SumConnector::REDISListLen:: key '" + sum_name + "' does not exist (yet?)");
    }
    else if (redis_reply == REDIS_REPLY_STRING) {
        result = strtold(result_string.c_str(), NULL);
        // Error while parsing integer
        if(errno == ERANGE) {
            DARWIN_LOG_ERROR("SumConnector::REDISListLen:: overflow/underflow while trying to parse value");
            result = 0.0L;
        }
    }
    else {
        DARWIN_LOG_ERROR("SumConnector::REDISListLen:: Error while querying key '" + sum_name + "'");
        return -1;
    }

    // Return an absolute rounded value for the double, cap with the maximum value of a long long int
    return (long long int)std::min(std::abs(std::round(result)), (long double)std::numeric_limits<long long int>::max());
}


bool SumConnector::FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &res) {
    res.clear();
    if(logs.size() == 1) {
        res = "[[" + logs[0] + ",\"" + darwin::time_utils::GetTime() + "\"" + "]]";
    }

    return not res.empty();
}