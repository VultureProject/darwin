/// \file     AThread.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "../../toolkit/RedisManager.hpp"

class AThread {
    /// This abstract class is made to be inheritated by subclasses
    /// Its purpose is to handle everything needed to run a thread.
    /// It is likely to be called by AThreadManager or any subclass inheriting from it.
    /// ThreadMain is calling Main every _interval seconds.
    /// Main MUST be overrode by children.
    ///
    ///\class AThread

public:
    ///\brief Unique constructor, creates a thread and immediately calls ThreadMain.
    ///
    ///\param interval The interval in seconds between two calls of Main by ThreadMain
    AThread(int interval);

    ///\brief Default virtual destructor 
    virtual ~AThread() = default;

    ///\brief initiates the _thread member
    void InitiateThread();

    ///\brief Stops the thread (with join)
    ///
    ///\return true on success, false otherwise
    bool Stop();

    ///\brief It is the entry point of the thread, it calls Main every _interval seconds and is called in the constructor.
    void ThreadMain();

    ///\brief This function is called every _interval seconds, and MUST be overrode by children
    ///
    ///\return Override MUST return true on success and false otherwise
    virtual bool Main() = 0;

protected:
    /// Interval, set by the ctor, between two calls of Main by ThreadMain
    int _interval;

private:
    /// The actual thread
    std::thread _thread;
    
    /// A boolean to know if the thread is stopped (true) or not (false).
    /// Set true in constructor
    /// Set false in destructor and in case an error occured
    std::atomic<bool> _is_stop;

    /// The condition variable for the thread
    std::condition_variable _cv;
};