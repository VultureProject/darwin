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

#define DARWIN_FILTER_PYTHON_EXAMPLE 0x70797468

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class PythonExampleTask : public darwin::Session {
public:
    explicit PythonExampleTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            PyObject *py_function);

    ~PythonExampleTask() override = default;

    static constexpr int KELVIN_TEMP_THRESHOLD = 300;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Read a struct in_addr from the session and
    /// lookup in the bad host map to fill _result.
    ///
    /// \return the certitude of host's bad reputation (100: BAD, 0:Good)
    unsigned int WarmWeatherDetector(const int fahrenheit_temperature) noexcept;

    /// Parse the body received.
    bool ParseBody() override;

private:
    int _current_fahrenheit_temp; //The host to lookup
    PyObject *_py_function = nullptr; // the Python function to call in the module
    std::vector<int> _fahrenheit_temps;
};
