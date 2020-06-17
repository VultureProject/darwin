/// \file     AThreadManager.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "AThread.hpp"


class AThreadManager {
public:
    AThreadManager(int nb_thread);
    ~AThreadManager();

public:
    /// Start the thread
    ///
    /// \return true in success, else false
    /// \param interval the number of seconds between 2 wake-ups (in seconds), defaults to 300
    bool ThreadStart();

    /// Stop the thread
    ///
    /// \return true in success, else false
    bool Stop();

private:
    virtual std::shared_ptr<AThread> Start() = 0;

    unsigned long _nb_threads; // TODO : Change for _nb_threads
    std::vector<std::shared_ptr<AThread>> _threads;
};