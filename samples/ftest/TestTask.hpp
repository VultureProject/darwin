/// \file     TestTask.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     03/12/2013
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "protocol.h"
#include "Session.hpp"

#define DARWIN_FILTER_TEST 0x74657374

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class TestTask : public darwin::Session {
public:
    explicit TestTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex);

    ~TestTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse a line from the body.
    bool ParseLine(rapidjson::Value &line) final;

private:
    std::string _line;
};
