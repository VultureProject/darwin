/// \file     BufferThread.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     17/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include <thread>
#include <vector>

#include "Connectors.hpp"
#include "AThread.hpp"

class BufferThread : public AThread {
    /// This calss is inheritating from AThread (AThread.hpp)
    /// Its purpose is to add to everything needed to handle a thread (implemented by AThread)
    /// what is specific to Buffer Filter threads.
    ///
    ///\class BufferThread
    
    public:
    ///\brief Unique constructor, needs an AConnector to setup BufferThread fields and to send interval to AThread
    ///
    ///\param output The connector needed to perform output Filter related actions.
    BufferThread(std::shared_ptr<AConnector> output);

    ///\brief virtual default destructor
    virtual ~BufferThread() override = default;

    private:
    ///\brief Entry point (called by AThread's ThreadMain function) every _interval seconds.
    /// Must override AThread's Main function.
    /// This function checks on Redis if there is enough logs on the associated _redis_list. (Given by Connector)
    /// If needed, it tries to pick the logs in REDIS.
    /// On success it sends them to the output Filter.
    /// On failure, it reiserts the logs into REDIS.
    ///
    ///\return true on success, false otherwise.
    virtual bool Main() override final;

    private:
    /// The connector to ensure link with the output Filter.
    std::shared_ptr<AConnector> _connector;

    /// The Redis Lists on which to write and pickup datas for the ouptut Filter.
    std::vector<std::pair<std::string, std::string>> _redis_lists;
};