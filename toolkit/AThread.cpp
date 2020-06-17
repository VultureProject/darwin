/// \file     AThread.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "AThread.hpp"
#include "Logger.hpp"

AThread::AThread(int interval) : 
                _interval(interval),
                _thread(std::thread(&AThread::ThreadMain, this)),                
                _is_stop(false)
{}

void AThread::ThreadMain() {
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

bool AThread::Stop() {
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