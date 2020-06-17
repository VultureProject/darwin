/// \file     AThread.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <any>
#include <string>
#include <thread>

#include "../../toolkit/RedisManager.hpp"

class AThread {
    public:
    AThread(int interval);
    virtual ~AThread() = default;

    bool Stop();

    /// The loop in which the thread main will be executed every _interval seconds
    void ThreadMain();
    virtual bool Main() = 0;

    private:
    int _interval; // Interval between 2 wake-ups in seconds.
    std::thread _thread; // The actual thread
    std::atomic<bool> _is_stop{true}; // To know if the thread is stopped or not
    std::condition_variable cv;
    std::mutex _mutex; // The mutex used to manage multiple acces to the _thread member
};