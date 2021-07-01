/// \file     ThreadManager.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "BufferThread.hpp"
#include "Logger.hpp"
#include "AlertManager.hpp"

BufferThread::BufferThread(std::shared_ptr<AConnector> output) :
                AThread(output->GetInterval()),
                _connector(output),
                _redis_lists(output->GetRedisLists()) {}


bool BufferThread::Main() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::Main:: Begin");

    for (auto &redis_config : this->_redis_lists) {
        std::string redis_list = redis_config.second;
        long long int len = this->_connector->REDISListLen(redis_list);
        DARWIN_LOG_DEBUG("BufferThread::Main:: There are " + std::to_string(len) + " entries in " + redis_list + " redis list.");
        std::vector<std::string> logs;

        if (len >= 0 && len < this->_connector->GetRequiredLogLength()){
            DARWIN_LOG_DEBUG("BufferThread::Main:: Not enough log in Redis, wait for more");
        } else if (len < 0 || !this->_connector->REDISPopLogs(len, logs, redis_list)) {
            DARWIN_LOG_ERROR("BufferThread::Main:: Error when querying Redis on list: " + redis_list + " for source: '" + redis_config.first + "'");
            continue;
        } else {
            if (not _connector->SendToFilter(logs)) {
                DARWIN_LOG_INFO("BufferThread::Main:: unable to send data to next filter, reinserting logs in redis ...");
                this->_connector->REDISReinsertLogs(logs, redis_list);
            } else {
                DARWIN_LOG_DEBUG("BufferThread::Main:: Removed " + std::to_string(logs.size()) + " elements from redis");
            }
        }

        // Set an expiration on the redis key, to purge if threads/filters are stopped or configuration is modified
        // The expiration MUST be over the interval period
        this->_connector->REDISSetExpiry(redis_list, this->_interval + 60);
    }

    return true;
}