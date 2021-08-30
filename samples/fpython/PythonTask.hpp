/// \file     PythonExample.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/PythonUtils.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "protocol.h"
#include "Session.hpp"
#include "Generator.hpp"

#define DARWIN_FILTER_PYTHON_EXAMPLE 0x70797468
#define DARWIN_FILTER_NAME "python"
#define DARWIN_ALERT_RULE_NAME "Python"
#define DARWIN_ALERT_TAGS "[]"

class PythonTask : public darwin::Session {
public:
    explicit PythonTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                         std::mutex& cache_mutex, FunctionHolder& functions);

    ~PythonTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    // xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    
    bool ParseLine(rapidjson::Value& line) override;

    /// Parse the body received.
    bool ParseBody() override;

    std::string GetFormated(FunctionPySo<FunctionHolder::format_t>& func, PyObject* processedData);

private:
    FunctionHolder& _functions;

    PyObject* _result;
    PyObject* _parsed_body;
};
