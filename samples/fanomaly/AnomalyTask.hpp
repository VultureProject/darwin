/// \file     AnomalyTask.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include <mlpack/core.hpp>
#include <mlpack/methods/pca/pca.hpp>
#include <mlpack/methods/dbscan/dbscan.hpp>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>

#include "protocol.h"
#include "Session.hpp"

#include "../../toolkit/lru_cache.hpp"

#define DARWIN_FILTER_ANOMALY 0x414D4C59

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class AnomalyTask: public darwin::Session {
public:
    explicit AnomalyTask(boost::asio::local::stream_protocol::socket& socket,
                           darwin::Manager& manager,
                           std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache);
    ~AnomalyTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Generate the logs from the anomalies found
    void GenerateLogs(std::vector<std::string> ips, arma::uvec index_anomalies, arma::mat alerts);

    /// The anomaly detection function
    bool Detection();

    /// Parse a line from the json body.
    bool ParseLine(rapidjson::Value &cluster);

private:
    // Indices of values in matrix (see variable "_data" below)
    static constexpr int UDP_NB_HOST  = 0;
    static constexpr int UDP_NB_PORT  = 1;
    static constexpr int TCP_NB_HOST  = 2;
    static constexpr int TCP_NB_PORT  = 3;
    static constexpr int ICMP_NB_HOST = 4;
    static constexpr int DISTANCE     = 5;

    static constexpr int MIN_DATA     = 10;

    // contain matrix that contain the data
    // data in matrix :                       ip1    ip2
    //                                      (   2      0       <--- UDP_NB_HOST
    //                                         40      0       <--- UDP_NB_PORT
    //                                         10      1       <--- TCP_NB_HOST
    //                                         10      3       <--- TCP_NB_PORT
    //                                          0     15   )  <--- ICMP_NB_HOST
    arma::mat _matrix;
    // ips linked to pre-processed data :   [  ip1,   ip2   ]
    std::vector<std::string> _ips;
};
