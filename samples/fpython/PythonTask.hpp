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
#include "ATask.hpp"
#include "DarwinPacket.hpp"
#include "ASession.fwd.hpp"
#include "Generator.hpp"
#include "fpython.hpp"

#define DARWIN_FILTER_PYTHON 0x70797468
#define DARWIN_FILTER_NAME "python"
#define DARWIN_ALERT_RULE_NAME "Python"
#define DARWIN_ALERT_TAGS "[]"

class PythonTask : public darwin::ATask {
public:
    explicit PythonTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            darwin::session_ptr_t s,
                            darwin::DarwinPacket& packet,
                            PyObject * pClass, 
                            FunctionHolder& functions);

    ~PythonTask() override;

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
    std::vector<std::string> GetFormatedAlerts(FunctionPySo<FunctionHolder::alert_format_t>& func, PyObject* processedData);
    DarwinResponse GetFormatedResponse(FunctionPySo<FunctionHolder::resp_format_t>& func, PyObject* processedData);

private:
    PyObject* _pClass;
    FunctionHolder& _functions;

    PyObjectOwner _pSelf;
    PyObjectOwner _parsed_body;
};
