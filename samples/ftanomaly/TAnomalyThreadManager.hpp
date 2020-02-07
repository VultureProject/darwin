/// \file     AnomalyThreadManager.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <any>
#include <string>
#include <thread>
#include <mlpack/core.hpp>
#include <mlpack/methods/pca/pca.hpp>
#include <mlpack/methods/dbscan/dbscan.hpp>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>

#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "../../toolkit/ThreadManager.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../../toolkit/FileManager.hpp"

class AnomalyThreadManager: public darwin::toolkit::ThreadManager{
public:
    AnomalyThreadManager(std::string& redis_internal);
    ~AnomalyThreadManager() override = default;

private:
    // Indices of values in matrix (see variable "_data" below)
    static constexpr int UDP_NB_HOST  = 0;
    static constexpr int UDP_NB_PORT  = 1;
    static constexpr int TCP_NB_HOST  = 2;
    static constexpr int TCP_NB_PORT  = 3;
    static constexpr int ICMP_NB_HOST = 4;
    static constexpr int DISTANCE     = 5;

    static constexpr int MIN_LOGS_LINES = 10;

    /// Main function
    bool Main() override;

    /// The algorithm to detect anomalies in data
    void Detection();

    /// Pre-process data and stock it in a matrix,
    /// before the algorithm process
    void PreProcess(std::vector<std::string> logs);

    /// Write the alerts/logs in Redis
    /// \return true on success, false otherwise.
    //TODO legacy function bound to disappear
    bool WriteRedis(const std::string& log_line);

    /// Used by "PreProcess" to process a log's line
    void PreProcessLine(const std::string& ip, const std::string& protocol, std::string port,
                        tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_data,
                        tsl::hopscotch_map<std::string, std::array<int, 5>>& data);

    /// Query the Redis to get length of the list
    /// where we have our data
    /// \return true on success, false otherwise.
    long long int REDISListLen() noexcept;

    /// Get the logs from the Redis
    /// \param rangeLen the range of logs we want from the redis list
    /// \param logs the vector where we want to stock our logs
    /// \return true on success, false otherwise.
    bool REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept;

    /// Remove the already processed logs in Redis
    /// \param rangeLen the range of logs we want to tirm in redis
    /// \return true on success, false otherwise.
    bool REDISReinsertLogs(std::vector<std::string> &logs) noexcept;

private:
    // pre-processed data put in a matrix :    ip1    ip2
    //                                      (   2      0       <--- UDP_NB_HOST
    //                                          40     0       <--- UDP_NB_PORT
    //                                          10     1       <--- TCP_NB_HOST
    //                                          10     3       <--- TCP_NB_PORT
    //                                          0      15   )  <--- ICMP_NB_HOST
    arma::mat _matrix = {};
    // ips linked to pre-processed data :   [  ip1,   ip2   ]
    std::vector<std::string> _ips;

    const std::string _redis_internal;
};
