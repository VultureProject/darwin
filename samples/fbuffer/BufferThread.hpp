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
    public:
    BufferThread(std::shared_ptr<AConnector> output);
    virtual ~BufferThread() override = default;

    public:

    private:
    /// Main function
    virtual bool Main() override final;

    /// Get the logs from the Redis
    /// \param rangeLen the range of logs we want from the redis list
    /// \param logs the vector where we want to stock our logs
    /// \return true on success, false otherwise.
    bool REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept;

    /// Query the Redis to get length of the list
    /// where we have our data
    /// \return true on success, false otherwise.
    long long int REDISListLen() noexcept;


    private:
    std::shared_ptr<AConnector> _connector;
    std::string _redis_list;
};