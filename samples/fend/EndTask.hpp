/// \file     EndTask.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include "protocol.h"
#include "Session.hpp"

#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/lru_cache.hpp"

#define DARWIN_FILTER_END 0x454E4453

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class EndTask: public darwin::Session {
public:
    explicit EndTask(boost::asio::local::stream_protocol::socket& socket,
                                       darwin::Manager& manager,
                                       std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                                       std::string redis_socket_path);

    ~EndTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Parse the line received. Useless for this filter.
    bool ParseLine(rapidjson::Value& line) final {}

    /// Add to REDIS the evt id and the certitude list size received by the filter
    ///
    /// \return true on success, false otherwise.
    bool REDISAdd(const std::string& evt_id, const std::string& nb_result) noexcept;

private:
    darwin::toolkit::RedisManager _redis_manager;
};
