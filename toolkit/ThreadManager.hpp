/// \file     ThreadManager.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     09/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <thread>
#include <condition_variable>
#include <atomic>

/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit {

        class ThreadManager {
        public:
            ThreadManager();

            virtual ~ThreadManager();

        public:
            /// Start the thread
            ///
            /// \return true in success, else false
            /// \param interval the number of seconds between 2 wake-ups (in seconds), defaults to 300
            bool Start(unsigned int interval = 300);

            /// Stop the thread
            ///
            /// \return true in success, else false
            bool Stop();

        protected:
            /// The loop in which the thread main will be executed every _interval seconds
            void ThreadMain();

            /// The function executed in the thread main
            ///
            /// \return true in success, else false
            virtual bool Main() = 0;

        private:
            std::thread _thread;
            std::atomic<bool> _is_stop{true}; // To know if the thread is stopped or not
            std::condition_variable cv;
            std::mutex _thread_mutex; // The mutex used to manage multiple acces to the _thread member

        protected:
            unsigned int _interval; // Interval in which the thread main function will be executed (in seconds)

        };
    }
}
