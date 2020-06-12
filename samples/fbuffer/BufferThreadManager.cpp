/// \file     BufferThreadManager.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>
#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "BufferThreadManager.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "Time.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

BufferThread::BufferThread(std::shared_ptr<AConnector> output) : 
                _interval(output->getInterval()),
                _thread(std::thread(&BufferThread::ThreadMain, this)),
                _is_stop(false), 
                _connector(output),
                _redis_list(output->getRedisList()) {}

void BufferThread::ThreadMain() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ThreadManager::ThreadMain:: Begin");
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);

    while (!(this->_is_stop)) {
        if (!Main()) {
            DARWIN_LOG_DEBUG("ThreadManager::ThreadMain:: Error in main function, stopping the thread");
            _is_stop = true;
            break;
        }
        // Wait for notification or until timeout
        cv.wait_for(lck, std::chrono::seconds(_interval));
    }
}

bool BufferThread::Main() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Begin");

    long long int len = REDISListLen();
    DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: There are " + std::to_string(len) + " entries in " + this->_redis_list + " redis list.");
    std::vector<std::string> logs;

    if (len>=0 && len<MIN_LOGS_LINES){
        DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Not enough log in Redis, wait for more");
        return true;
    } else if (len<0) {// || !REDISPopLogs(len, logs)) {
        DARWIN_LOG_ERROR("BufferThread::ThreadMain:: Error when querying Redis");
        return false;
    } else {
//        _connector->sendData(logs);
        DARWIN_LOG_DEBUG("BufferThread::ThreadMain:: Removed" + std::to_string(logs.size()) + " elements from redis");
    }

    for (auto &log : logs) {
        DARWIN_RAISE_ALERT(log);
    }

    return true;
}

bool BufferThread::Stop() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ThreadManager:: Stop thread");

    std::lock_guard<std::mutex> lck(_mutex);
    this->_is_stop = true;
    //Notify the thread
    cv.notify_all();
    try {
        if (_thread.joinable()) {
            _thread.join();
        }
    } catch (const std::system_error &e) {
        DARWIN_LOG_WARNING("ThreadManager:: Error when trying to stop the thread");
        DARWIN_LOG_WARNING("ThreadManager:: Error message: " + e.code().message());
        return false;
    }
    DARWIN_LOG_DEBUG("ThreadManager:: Thread stopped");
    return true;
}

//TODO Legacy function, bound to disapear
/*
bool BufferThread::WriteRedis(const std::string& log_line){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::WriteRedis:: Starting writing alerts in Redis...");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(not _redis_internal.empty()) {
        DARWIN_LOG_DEBUG("BufferThread::WriteRedis:: Writing to Redis list: " + _redis_internal);
        if(redis.Query(std::vector<std::string>{"SADD", _redis_internal, log_line}, true) == REDIS_REPLY_ERROR) {
            DARWIN_LOG_WARNING("BufferThreadManager::REDISAddLogs:: Failed to add log in Redis !");
            return false;
        }
    }
    return true;
}
*/
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
/*
bool BufferThread::REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::REDISPopLogs:: Querying Redis for logs...");

    std::any result;
    std::vector<std::any> result_vector;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SPOP", _redis_internal, std::to_string(len)}, result, true) != REDIS_REPLY_ARRAY) {
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

bool BufferThread::REDISReinsertLogs(std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThread::REDISReinsertLogs:: Querying Redis to reinsert logs...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_internal);

    for (std::string& log: logs){
        arguments.emplace_back(log);
    }

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("BufferThread::REDISReinsertLogs:: Not the expected Redis response");
        return false;
    }

    DARWIN_LOG_DEBUG("BufferThread::REDISReinsertLogs:: Reinsertion done");
    return true;
}
*/
BufferThreadManager::BufferThreadManager(int max_nb_threads, std::string& redis_internal) :
        _max_nb_threads(max_nb_threads),
        _redis_internal(redis_internal) {

    if (not _redis_internal.empty()) {
        _is_log_redis = true;
    }
}

bool BufferThreadManager::Start(std::shared_ptr<AConnector> output) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ThreadManager:: Starting threads");

//  std::lock_guard<std::mutex> lck(thread.getMutex())
    if (this->_threads.size() > this->_max_nb_threads) {
        DARWIN_LOG_WARNING("ThreadManager:: The maximum number of threads already reached.");
        return false;
    }
    try {
        auto ptr = std::make_shared<BufferThread>(output);
        _threads.push_back(ptr);
    } catch (const std::system_error &e) {
        DARWIN_LOG_WARNING("ThreadManager:: Error when starting the thread");
        DARWIN_LOG_WARNING("ThreadManger:: Error message: " + e.code().message());
        return false;
    }
    DARWIN_LOG_DEBUG("ThreadManager:: All threads started");
    return true;
}

bool BufferThreadManager::Stop() {
    bool ret = true;
    for (auto thread : this->_threads) {
        if (!thread->Stop())
            ret = false;
    }
    return ret;
}

BufferThreadManager::~BufferThreadManager() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("BufferThreadManager::~BufferThreadManager Preparing to shut down all the threads");
    Stop();
}
    