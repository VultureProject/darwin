/// \file     LogsTask.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "protocol.h"
#include "Session.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/FileManager.hpp"
#include "../../toolkit/RedisManager.hpp"

#define DARWIN_FILTER_LOGS 0x4C4F4753

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class LogsTask: public darwin::Session {
public:
    explicit LogsTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager,
                      std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                      std::mutex& cache_mutex,
                      bool log,
                      bool redis,
                      std::string& log_file_path,
                      std::shared_ptr<darwin::toolkit::FileManager>& log_file,
                      std::string& redis_list_name,
                      std::string& redis_channel_name);
    ~LogsTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse the body received.
    bool ParseBody() override;

    bool ParseLine(rapidjson::Value &line) final {}

    /// Write the logs in file
    bool WriteLogs();

    /// Write the logs in REDIS
    bool REDISAddLogs(const std::string& logs);


private:

    static constexpr unsigned int RETRY = 1;
    bool _log; // If the filter will stock the data in a log file
    bool _redis; // If the filter will stock the data in a REDIS
    std::string _log_file_path;
    std::string _redis_list_name;
    std::string _redis_channel_name;
    std::shared_ptr<darwin::toolkit::FileManager> _log_file = nullptr;
};
