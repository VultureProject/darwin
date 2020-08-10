/// \file     AThreadManager.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "AThread.hpp"
 

class AThreadManager {
    /// This abstract class is made to be inheritated by subclasses
    /// Its purpose is to handle everything needed to run multiple AThread instances (AThread.hpp).
    /// Start MUST be override. It creates an AThread (or child) instance and return it as a pointer.
    /// It can handle AThreads, and any subclasses of it by overriding Start to create the correct type of AThread child.
    /// ThreadStart is calling Start if possible.
    ///
    ///\class AThreadManager

    public:
    ///\brief Unique constructor. Does not create the AThreads
    ///
    ///\param nb_thread The number of threads that the ThreadManager can handle.
    AThreadManager(int nb_thread);

    ///\brief call Stop
    ~AThreadManager();

    public:
    ///\brief calls Start if there are less than _nb_threads Threads already created
    ///
    ///\return true on success, false otherwise
    bool ThreadStart();

    ///\brief Stop all the threads
    ///
    ///\return true in success, else false
    bool Stop();

    private:
    ///\brief Starts a Thread. MUST be override by children to create the appropriate type of AThread children
    ///
    ///\return a shared_ptr on the newly created AThread child.
    virtual std::shared_ptr<AThread> Start() = 0;

    /// The number of threads handled by the ThreadManager. Set in the constructor
    unsigned long _nb_threads;

    /// A vector containing all the threads
    std::vector<std::shared_ptr<AThread>> _threads;
};