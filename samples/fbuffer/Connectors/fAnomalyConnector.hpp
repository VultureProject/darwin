/// \file     fAnomalyConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <mlpack/core.hpp>
#include <mlpack/methods/pca/pca.hpp>
#include <mlpack/methods/dbscan/dbscan.hpp>
#include <mlpack/methods/neighbor_search/neighbor_search.hpp>

#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "AConnector.hpp"

static constexpr int UDP_NB_HOST  = 0;
static constexpr int UDP_NB_PORT  = 1;
static constexpr int TCP_NB_HOST  = 2;
static constexpr int TCP_NB_PORT  = 3;
static constexpr int ICMP_NB_HOST = 4;
static constexpr int DISTANCE     = 5;

class fAnomalyConnector final : public AConnector {
    /// This class inherits from AConnector (see AConnector.hpp)
    /// It provides correct info picking to send to Anomaly type filter.
    /// 
    ///\class fAnomalyConnector

    public:
    ///\brief Unique constructor. It contains all stuff needed to ensure REDIS and output Anomaly Filter communication
    ///
    ///\param io_context The boost::asio::io_context used by the Server. Needed for communication with output Filter.
    ///\param filter_socket_path The socket path used to connect with output filter.
    ///\param interval Interval between two data sendings to output filter if there are enough logs in redis_list REDIS storage.
    ///\param redis_lists The names of the Redis List on which the connector will store and retrieve data depending to source, before sending to output Filter
    ///\param required_log_lines The number of logs required before sending data to output Filter
    fAnomalyConnector(boost::asio::io_context &context,
                        std::string &filter_socket_path,
                        unsigned int interval,
                        std::vector<std::pair<std::string, std::string>> &redis_lists,
                        unsigned int required_log_lines);

    ///\brief Virtual final default constructor
    virtual ~fAnomalyConnector() override final = default;

    public:
    ///\brief This functions sends datas to the REDIS storage. It override default pure virtual one as each filter doesn't need the same datas.
    ///
    /// It should fill _entry with the datas to send as REDISAddEntry is picking in it.
    ///
    ///\param input_line is a map representing all the entry received by the BufferTask.
    ///
    ///\return true on success, false otherwise.
    virtual bool sendToRedis(std::map<std::string, std::string> &input_line) override final;

    private:
    ///\brief this virtual function override AConnector's one to perform specific modifications to data.
    /// 
    ///\param logs The logs to jsonify
    ///
    ///\return a string representing logs under a json form.
    virtual bool FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &formatted) override final;

    /// Pre-process data and stock it in a matrix,
    /// before the algorithm process
    tsl::hopscotch_map<std::string, std::array<int, 5>> PreProcess(const std::vector<std::string> &logs);

    /// Used by "PreProcess" to process a log's line
    void PreProcessLine(const std::string& ip, const std::string &ip_dst, const std::string& protocol, std::string port,
                        tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_port,
                        tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_ip,
                        tsl::hopscotch_map<std::string, std::array<int, 5>>& data);

    // ips linked to pre-processed data :   [  ip1,   ip2   ]
    std::vector<std::string> _ips;

};