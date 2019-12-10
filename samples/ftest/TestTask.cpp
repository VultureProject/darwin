/// \file     TestTask.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     03/12/2013
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string>
#include <string.h>
#include <thread>

#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "TestTask.hpp"
#include "Logger.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

TestTask::TestTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex)
        : Session{"test", socket, manager, cache, cache_mutex} {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t TestTask::GenerateHash() {
    return xxh::xxhash<64>(_line, _line.length());
}

long TestTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_TEST;
}

void TestTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &line : array) {
        SetStartingTime();
        xxh::hash64_t hash;

        if(ParseLine(line)) {
            DARWIN_RAISE_ALERT(_line);
            _certitudes.push_back(0);
        }
        else {
            _certitudes.push_back(DARWIN_ERROR_RETURN);
        }
    }
}

bool TestTask::ParseLine(rapidjson::Value& line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: You must provide only the host in the list");
        return false;
    }

    if (not values[0].IsString()) {
        DARWIN_LOG_ERROR("TestTask:: ParseBody: The host sent must be a string");
        return false;
    }

    _line = values[0].GetString();

    if (_line == "trigger_ParseLine_error")
        return false;

    return true;
}
