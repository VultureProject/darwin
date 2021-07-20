/// \file     TestTask.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     11/12/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"

#define DARWIN_FILTER_TEST 0x74657374
#define DARWIN_FILTER_NAME "test"
#define DARWIN_ALERT_RULE_NAME "Test"
#define DARWIN_ALERT_TAGS "[\"default_test_tag_0\",  \"default_test_tag_1\"]"

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class TestTask : public darwin::ATask {
public:
    explicit TestTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            darwin::session_ptr_t s,
                            darwin::DarwinPacket& packet,
                            std::string& list,
                            std::string& channel);

    ~TestTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

    /// Adds a line to a test list in redis
    bool REDISAddList(const std::string& list, const std::string& line);

    /// Publish a line to a test channel in redis
    bool REDISPublishChannel(const std::string& channel, const std::string& line);

private:
    /// Parse a line from the body.
    bool ParseLine(rapidjson::Value &line) final;

private:
    std::string _line;
    std::string _redis_list;
    std::string _redis_channel;
};
