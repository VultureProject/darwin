/// \file     ThreadManager.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>
#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "BufferThread.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"

BufferThread::BufferThread(std::shared_ptr<AConnector> output) :
                AThread(output->getInterval()),
                _connector(output),
                _redis_list(output->getRedisList()) {}


bool BufferThread::Main() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Begin");

    long long int len = REDISListLen();
    DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: There are " + std::to_string(len) + " entries in " + this->_redis_list + " redis list.");
    std::vector<std::string> logs;

    if (len >= 0 && len < this->_connector->getRequiredLogLength()){
        DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Not enough log in Redis, wait for more");
        return true;
    } else if (len<0 || !REDISPopLogs(len, logs)) {
        DARWIN_LOG_ERROR("BufferThread::ThreadMain:: Error when querying Redis");
        return false;
    } else {
//        _connector->sendData(logs);
        DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Removed" + std::to_string(logs.size()) + " elements from redis");
    }

    for (auto &log : logs) { // TODO : bound to disappear, to be remplaced by sending data to filter in above else statement
        DARWIN_RAISE_ALERT(log);
    }

    return true;
}

long long int BufferThread::REDISListLen() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::REDISListLen:: Querying Redis for list size...");

    long long int result;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SCARD", _redis_list}, result, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("BufferThread::REDISListLen:: Not the expected Redis response");
        return -1;
    }
    return result;
}

bool BufferThread::REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::REDISPopLogs:: Querying Redis for logs...");

    std::any result;
    std::vector<std::any> result_vector;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SPOP", this->_redis_list, std::to_string(len)}, result, true) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("BufferThread::REDISPopLogs:: Not the expected Redis response");
        return false;
    }

    DARWIN_LOG_DEBUG("BufferThread::REDISPopLogs:: Elements removed successfully");

    try {
        result_vector = std::any_cast<std::vector<std::any>>(result);
    }
    catch (const std::bad_any_cast&) {}

    DARWIN_LOG_DEBUG("Got " + std::to_string(result_vector.size()) + " entries from Redis");

    for(auto& object : result_vector) {
        try {
            logs.emplace_back(std::any_cast<std::string>(object));
        }
        catch(const std::bad_any_cast&) {}
    }

    return true;
}