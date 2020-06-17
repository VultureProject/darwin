/// \file     AThreadManager.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "AThreadManager.hpp"
#include "Logger.hpp"

AThreadManager::AThreadManager(int nb_threads) :
        _nb_threads(nb_threads) {}

bool AThreadManager::ThreadStart() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AThreadManager:: Starting threads");

    if (this->_threads.size() > this->_nb_threads) {
        DARWIN_LOG_WARNING("AThreadManager:: The maximum number of threads already reached.");
        return false;
    }
    try {
        auto th = this->Start();
        this->_threads.push_back(th);
    } catch (const std::system_error &e) {
        DARWIN_LOG_WARNING("AThreadManager:: Error when starting the thread");
        DARWIN_LOG_WARNING("ThreadManger:: Error message: " + e.code().message());
        return false;
    }
    DARWIN_LOG_DEBUG("AThreadManager:: All threads started");
    return true;
}

bool AThreadManager::Stop() {
    bool ret = true;
    for (auto thread : this->_threads) {
        if (!thread->Stop())
            ret = false;
    }
    return ret;
}

AThreadManager::~AThreadManager() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AThreadManager::~AThreadManager Preparing to shut down all the threads");
    if (not this->Stop()) {
        DARWIN_LOG_ERROR("AThreadManager::~AThreadManager At least one thread didn't stop correctly");
    } else {
        DARWIN_LOG_DEBUG("AThreadManager::~AThreadManager All threads stopped correctly");
    }
}