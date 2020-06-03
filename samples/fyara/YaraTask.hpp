/// \file     Yara.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "../toolkit/Yara.hpp"
#include "Session.hpp"

#define DARWIN_FILTER_YARA_SCAN 0x79617261

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class YaraTask : public darwin::Session {
public:
    explicit YaraTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            std::shared_ptr<darwin::toolkit::YaraEngine> yaraEngine);

    ~YaraTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse the body received.
    bool ParseLine(rapidjson::Value &line) final;

private:
    std::string _chunk;
    std::shared_ptr<darwin::toolkit::YaraEngine> _yaraEngine = nullptr;
};
