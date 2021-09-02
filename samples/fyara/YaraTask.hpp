/// \file     toolkit/Yara.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "../toolkit/lru_cache.hpp"
#include "../toolkit/xxhash.h"
#include "../toolkit/xxhash.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "../../toolkit/rapidjson/writer.h"
#include "Encoders.h"

#include "Yara.hpp"
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"

#define DARWIN_FILTER_YARA_SCAN 0x79617261
#define DARWIN_FILTER_NAME "yara"
#define DARWIN_ALERT_RULE_NAME "Yara scanner"
#define DARWIN_ALERT_TAGS "[]"

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class YaraTask : public darwin::ATask {
public:
    explicit YaraTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            darwin::session_ptr_t s,
                            darwin::DarwinPacket& packet,
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

    /// Convert a std::set to string json list
    std::string GetJsonListFromSet(std::set<std::string> &input);

private:
    std::string _chunk;
    std::shared_ptr<darwin::toolkit::YaraEngine> _yaraEngine = nullptr;
};
