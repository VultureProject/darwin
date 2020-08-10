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
    /// This abstract class is made to be inheritated by subclasses
    /// Its purpose is to handle everything needed to run a thread.
    /// It is likely to be called by AThreadManager or ay subclass inheritating from it.
    /// ThreadMain is calling Main every _interval seconds.
    /// Main MUST be override by children.
    ///
    ///\class AThread

    public:
    ///\brief Unique constructor, creates a thread and immediatly call ThreadMain.
    ///
    ///\param interval The interval in seconds between to calls of Main by ThreadMain
    AThread(int interval);

    ///\brief Default virtual destructor 
    virtual ~AThread() = default;

    ///\brief Stops the thread (with join)
    ///
    ///\return true on success, false otherwise
    bool Stop();

    ///\brief It is the entry point of the thread, it calls Main every _interval seconds and is called in the constructor.
    void ThreadMain();

    ///\brief This function is called every _interval seconds, and MUST be override by children
    ///
    ///\return Override MUST return true on success and false otherwise
    virtual bool Main() = 0;


    private:
    /// Interval, set by the ctor, between two calls of Main by ThreadMain
    int _interval;

    /// The actual thread
    std::thread _thread;
    
    /// A boolean to know if the thread is stopped (true) or not (false).
    /// Set true by the construction
    /// Set false by destructor and in case of error
    std::atomic<bool> _is_stop;

    /// The condition variable for the thread
    std::condition_variable cv;

    /// The mutex used to manage multiple access to the _thread member
    std::mutex _mutex;
};