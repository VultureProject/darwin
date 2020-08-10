/// \file     ThreadManager.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include <thread>

#include "Connectors.hpp"
#include "AThreadManager.hpp"
#include "AThread.hpp"

class BufferThreadManager : public AThreadManager {
    /// This class is inheritating from AThreadManager which contains everything needed
    /// to create, run and handle several threads.
    /// This class is adding everything specific to BufferThreads.
    /// This class is overriding Start to create BufferThreads.
    ///
    ///\class BufferThreadManager

    public:
    ///\brief Unique constructor. Does not create the BufferThreads
    ///
    ///\param nb_thread The number of theads the Manager can handle.
    BufferThreadManager(int nb_thread);

    ///\brief default destructor (Parent class destructor stops the threads)
    virtual ~BufferThreadManager() = default;

    public:
    ///\brief Starts a BufferThread
    ///
    ///\return a shared_ptr on the newly created BufferThread
    virtual std::shared_ptr<AThread> Start() override final;

    ///\brief Sets _connector field.
    ///
    ///\param connector The connector to set in the field
    void setConnector(std::shared_ptr<AConnector> connector);

    private:
    /// The connector used to create a new Thread (as Start does not takes any parameter)
    std::shared_ptr<AConnector> _connector;
};