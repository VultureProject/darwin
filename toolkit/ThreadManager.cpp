/// \file     ThreadManager.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     09/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "ThreadManager.hpp"

#include <chrono>
#include <thread>
#include "base/Logger.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit {

        ThreadManager::ThreadManager() = default;

        bool ThreadManager::Start() {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("ThreadManager:: Start thread");

            if (!_is_stop) {
                DARWIN_LOG_DEBUG("ThreadManager:: Thread already started");
                return true;
            }

            _is_stop = false;
            try {
                _thread = std::thread(&ThreadManager::ThreadMain, this);
            } catch (const std::system_error &e) {
                _is_stop = true;
                DARWIN_LOG_WARNING("ThreadManager:: Error when starting the thread");
                DARWIN_LOG_WARNING("ThreadManger:: Error message: " + e.code().message());
                return false;
            }

            DARWIN_LOG_DEBUG("ThreadManager:: Thread started");
            return true;
        }

        bool ThreadManager::Stop() {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("ThreadManager:: Stop thread");

            if (_is_stop) {
                DARWIN_LOG_DEBUG("ThreadManager:: Thread already stopped");
                return true;
            }

            _is_stop = true;
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

        void ThreadManager::ThreadMain() {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("ThreadManager::ThreadMain:: Begin");
            std::mutex mtx;
            std::unique_lock<std::mutex> lck(mtx);

            while (!_is_stop) {
                if (!Main()) {
                    DARWIN_LOG_DEBUG("ThreadManager::ThreadMain:: Error in main function, stopping the thread");
                    _is_stop = true;
                    break;
                }
                //Wait for notification or until timeout
                cv.wait_for(lck, std::chrono::seconds(_interval));
            }
        }

        ThreadManager::~ThreadManager() {
            Stop();
        }
    }
}
