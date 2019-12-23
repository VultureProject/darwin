/// \file     RogueDevice.hpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     25/11/19
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

#define DARWIN_FILTER_ROGUE_DEVICE 0x72676476

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class RogueDeviceTask : public darwin::Session {
public:
    explicit RogueDeviceTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            std::mutex& cache_mutex,
                            PyObject *py_function,
                            std::string input_csv,
                            std::string output_csv,
                            std::string output_json);

    ~RogueDeviceTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse the body received.
    bool ParseBody() override;

    virtual bool ParseLine(rapidjson::Value& line, darwin::toolkit::FileManager& file) final;

private:
    PyObject *_py_function = nullptr; // the Python function to call in the module
    std::string _csv_input_path; //!< Python script input
    std::string _csv_ouput_path; //!< Python Script output
    std::string _json_output_path; //!< Python Script output
};
