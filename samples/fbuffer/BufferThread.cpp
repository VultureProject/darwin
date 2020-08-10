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
                _redis_lists(output->getRedisList()) {}


bool BufferThread::Main() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Begin");

    for (auto &redis_config : this->_redis_lists) {
        std::string redis_list = redis_config.second;
        long long int len = this->_connector->REDISListLen(redis_list);
        DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: There are " + std::to_string(len) + " entries in " + redis_list + " redis list.");
        std::vector<std::string> logs;

        if (len >= 0 && len < this->_connector->getRequiredLogLength()){
            DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Not enough log in Redis, wait for more");
            continue;
        } else if (len<0 || !this->_connector->REDISPopLogs(len, logs, redis_list)) {
            DARWIN_LOG_ERROR("BufferThread::ThreadMain:: Error when querying Redis");
            continue;
        } else {
            if (not _connector->sendToFilter(logs)) {
                DARWIN_LOG_ERROR("BufferThread::Main unable to send data to next filter, reinserting logs in redis ...");
                this->_connector->REDISReinsertLogs(logs, redis_list);
            } else {
                DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Removed " + std::to_string(logs.size()) + " elements from redis");
            }
        }
    }

    return true;
}