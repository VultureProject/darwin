/// \file     AnomalyThreadManager.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>
#include <vector>

#include "../../toolkit/RedisManager.hpp"
#include "TAnomalyThreadManager.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "Time.hpp"
#include "protocol.h"
#include "AlertManager.hpp"
#include "StringUtils.hpp"

AnomalyThreadManager::AnomalyThreadManager(std::string& redis_internal)
        :_redis_internal(redis_internal) {}

bool AnomalyThreadManager::Main(){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::ThreadMain:: Begin");

    long long int len = 0;
    std::vector<std::string> logs;

    len = REDISListLen();

    if (len>=0 && len<MIN_LOGS_LINES){
        DARWIN_LOG_DEBUG("AnomalyThread::ThreadMain:: Not enough log in Redis, wait for more");
        return true;
    } else if (len<0 || !REDISPopLogs(len, logs)){
        DARWIN_LOG_ERROR("AnomalyThread::ThreadMain:: Error when querying Redis");
        return true;
    } else{
        PreProcess(logs);
        if (_matrix.n_cols<10){
            DARWIN_LOG_DEBUG("AnomalyThread::ThreadMain:: Not enough log in Redis, wait for more");
            REDISReinsertLogs(logs);
            _matrix.reset();
        }else{
            Detection();
        }
    }

    return true;
}

void AnomalyThreadManager::Detection(){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::Detection:: Start detection ...");
    double epsilon = 100;
    const size_t newDataDimension = 2, sizePoint = 5;

    arma::Mat<size_t> neighbors;
    arma::Row<size_t> assignments;
    arma::uvec index_anomalies, index_no_anomalies;
    arma::mat anomalies, no_anomalies, distances, alerts;

    mlpack::pca::PCA<> pcaa;
    mlpack::dbscan::DBSCAN<> db(epsilon, sizePoint);

    alerts = _matrix;

    // reduce matrix' dimension from 5 to 2
    pcaa.Apply(_matrix, newDataDimension);
    db.Cluster(_matrix, assignments);

    no_anomalies = _matrix;
    anomalies    = _matrix;

    index_anomalies = arma::find(assignments == SIZE_MAX); // index of noises find by the clustering in the _matrix
    if (index_anomalies.is_empty()){
        DARWIN_LOG_DEBUG("AnomalyThread::Detection:: No anomalies found");
        return;
    }
    no_anomalies.shed_cols(index_anomalies); // suppress noisy columns, keep only normals data
    index_no_anomalies = arma::find(assignments != SIZE_MAX); // index noises find by the clustering in the _matrix
    anomalies.shed_cols(index_no_anomalies); // suppress non noisy-columns, keep only abnormal data

    //calculate distance between an abnormal ip and the nearest normal cluster
    mlpack::neighbor::NeighborSearch<mlpack::neighbor::NearestNeighborSort ,
            mlpack::metric::EuclideanDistance> ns(no_anomalies);
    ns.Search(anomalies, 1, neighbors, distances);

    alerts.shed_cols(index_no_anomalies);
    // add distance to alerts
    alerts.insert_rows(DISTANCE, distances);

    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        STAT_MATCH_INC;
        std::string details(R"({"ip": ")" + _ips[index_anomalies(i)] + "\","
                + R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ","
                + R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ","
                + R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ","
                + R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ","
                + R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ","
                + R"("distance": )" + std::to_string(alerts(DISTANCE, i))
                + "}");
        DARWIN_ALERT_MANAGER.Alert(_ips[index_anomalies(i)], 100, "-", details);
    }
    _matrix.reset();
}

void AnomalyThreadManager::PreProcess(std::vector<std::string> logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::PreProcess:: Starting the pre-process...");

    size_t size, pos, i;
    char delimiter = ';';
    std::array<int, 5> values{};
    std::string ip, ip_dst, port, protocol;
    std::vector<std::string> vec;

    // data to be pre-processed : {
    //                                  ip1 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ip2 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ...
    //                            }
    tsl::hopscotch_map<std::string, std::array<int, 5>> data;
    tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> cache_port;
    tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> cache_ip;

    for (const std::string &l : logs) {
        // line of log : ip_src;ip_dst;port;(udp|tcp) or  ip_src;ip_dst;0;icmp
        // where udp, tcp and icmp are represented by the ip protocol number
        try {
            vec = darwin::strings::SplitString(l, delimiter);

            if (vec.size() != 4) {
                DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (element missing), line ignored");
                continue;
            }
            ip = vec[0];
            ip_dst = vec[1];
            port = vec[2];
            protocol = vec[3];

            if (ip.empty() or ip_dst.empty() or protocol.empty() or (protocol != "1" and port.empty())) {
                DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (element empty), line ignored");
                continue;
            }
            PreProcessLine(ip, ip_dst, protocol, port, cache_port, cache_ip, data);

        } catch (const std::out_of_range& e) {
            std::string warning("fAnomalyConnector::PreProcess:: Error when parsing a log line , line ignored: ");
            warning += e.what();
            DARWIN_LOG_WARNING(warning);
            continue;
        }
    }
    size = data.size();
    // Set the size of the matrix and fill it w/ zeros
    // (see .hpp file to know this matrix's content)
    _matrix.zeros(5, size);
    i = 0;
    for(auto it = data.begin(); it != data.end(); ++it) {
        _ips.emplace_back(it->first);
        values = it.value();
        for(unsigned int j = 0; j<5; j++){
            _matrix(j, i) = values[j];
        }
        i++;
    }
}

void AnomalyThreadManager::PreProcessLine(const std::string& ip, const std::string &ip_dst, const std::string& protocol, const std::string& port,
                                          tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_port,
                                          tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_ip,
                                          tsl::hopscotch_map<std::string, std::array<int, 5>>& data) {
    tsl::hopscotch_set<std::string>* set_ip;
    tsl::hopscotch_set<std::string>* set_port;

    if(data.find(ip) != data.end()){

        if (protocol == "1") {
            set_ip = &cache_ip[ip + ":1"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][ICMP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }

        } else if (protocol == "17") {
            set_ip = &cache_ip[ip + ":17"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][UDP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }

            set_port = &cache_port[ip + ":17"];
            if(set_port->find(port) == set_port->end()) {
                data[ip][UDP_NB_PORT] += 1;
                set_port->emplace(port);
            }

        } else if (protocol == "6") {
            set_ip = &cache_ip[ip + ":6"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][TCP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }

            set_port = &cache_port[ip + ":6"];
            if(set_port->find(port) == set_port->end()) {
                data[ip][TCP_NB_PORT] += 1;
                set_port->emplace(port);
            }
        }
        return;
    }


    if (protocol == "1") {
        data.insert({ip, {0,0,0,0,1}});
        cache_ip[ip + ":1"].emplace(ip_dst);

    } else if (protocol == "17") {
        data.insert({ip, {1,1,0,0,0}});
        cache_ip[ip + ":17"].emplace(ip_dst);
        cache_port[ip + ":17"].emplace(port);

    } else if (protocol == "6") {
        data.insert({ip, {0,0,1,1,0}});
        cache_ip[ip + ":6"].emplace(ip_dst);
        cache_port[ip + ":6"].emplace(port);
    }
}


long long int AnomalyThreadManager::REDISListLen() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISListLen:: Querying Redis for list size...");

    long long int result;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SCARD", _redis_internal}, result, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISListLen:: Not the expected Redis response");
        return -1;
    }

    return result;
}

bool AnomalyThreadManager::REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISPopLogs:: Querying Redis for logs...");

    std::any result;
    std::vector<std::any> result_vector;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SPOP", _redis_internal, std::to_string(len)}, result, true) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISPopLogs:: Not the expected Redis response");
        return false;
    }

    try {
        result_vector = std::any_cast<std::vector<std::any>>(result);
    }
    catch (const std::bad_any_cast&) {}

    DARWIN_LOG_DEBUG("Got " + std::to_string(result_vector.size()) + " entries from Redis");

    for(auto& object : result_vector) {
        try {
            logs.emplace_back(std::any_cast<std::string>(object));
        }
        catch(const std::bad_any_cast&) {}
    }

    return true;
}

bool AnomalyThreadManager::REDISReinsertLogs(std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISReinsertLogs:: Querying Redis to reinsert logs...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_internal);

    for (std::string& log: logs){
        arguments.emplace_back(log);
    }

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISReinsertLogs:: Not the expected Redis response");
        return false;
    }

    DARWIN_LOG_DEBUG("AnomalyThread::REDISReinsertLogs:: Reinsertion done");
    return true;
}
