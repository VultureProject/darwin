/// \file     NSigmaTask.hpp
/// \authors  Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \version  1.0
/// \date     18/08/2021
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <map>
#include <set>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "protocol.h"
#include "Session.hpp"

#define DARWIN_FILTER_EXFILDATA 0x6e736967
#define DARWIN_FILTER_NAME "nsigma"
#define DARWIN_ALERT_RULE_NAME "N-Sigma Filter"
#define DARWIN_ALERT_TAGS "[]"

#define MINIMAL_SIZE 10

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class NSigmaTask : public darwin::Session {
public:
    explicit NSigmaTask(boost::asio::local::stream_protocol::socket& socket,
                     darwin::Manager& manager,
                     std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                     std::mutex& cache_mutex, int n_sigma);
    ~NSigmaTask() override;

public:
    // You need to override the functor to compile and be exeintcuted by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    // xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Parse a line in the body.
    bool ParseLine(rapidjson::Value &line) final;

    void CalculatePCR();

private:
    const int _n_sigma;
    std::vector<double> input_pcr;
    std::vector<double> filtered_pcr;
    std::vector<size_t> filtered_indices;
};
