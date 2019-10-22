/// \file     AnomalyTask.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include "protocol.h"
#include "Session.hpp"
#include "TAnomalyThreadManager.hpp"

#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"

#define DARWIN_FILTER_TANOMALY 0x544D4C59

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class AnomalyTask: public darwin::Session {
public:
    explicit AnomalyTask(boost::asio::local::stream_protocol::socket& socket,
                                       darwin::Manager& manager,
                                       std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                       std::shared_ptr<AnomalyThreadManager> vat,
                                       std::string redis_list_name);
    ~AnomalyTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse the body received.
    bool ParseBody() override;

    /// Parse the data received in the body.
    bool ParseData(const rapidjson::Value& data);

    /// Add the data parsed to REDIS
    /// \return true on success, false otherwise.
    bool REDISAdd(std::vector<std::string> values) noexcept;

private:
    bool _learning_mode = true;
    std::string _redis_list_name;
    std::shared_ptr<AnomalyThreadManager> _anomaly_thread_manager = nullptr;
};
