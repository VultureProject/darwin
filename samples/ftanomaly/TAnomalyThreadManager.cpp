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
#include "Time.hpp"
#include "protocol.h"

AnomalyThreadManager::AnomalyThreadManager(std::string& redis_internal,
                                            std::shared_ptr<darwin::toolkit::FileManager> log_file,
                                            std::string& redis_alerts_channel,
                                            std::string& redis_alerts_list)
        : _log_file{log_file},
        _redis_internal(redis_internal),
        _redis_alerts_channel(redis_alerts_channel),
        _redis_alerts_list(redis_alerts_list){
            if(not _redis_alerts_list.empty() or not _redis_alerts_channel.empty()) {
                _is_log_redis = true;
            }

            if(_log_file != nullptr) {
                _is_log_file = true;
            }
        }

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
        DARWIN_LOG_ERROR("AnomalyThread::ThreadMain:: Error when querying Redis, stopping the thread");
        return false;
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

    if(_is_log_file) {
        WriteLogs(index_anomalies, alerts);
    }

    if(_is_log_redis) {
        WriteRedis(index_anomalies, alerts);
    }
    _matrix.reset();
}

bool AnomalyThreadManager::WriteLogs(const arma::uvec index_anomalies, const arma::mat alerts){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::WriteAlerts:: Starting writing alerts in logs file...");

    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        std::string log_line(R"({"time": ")" + darwin::time_utils::GetTime() + R"(", "filter": "tanomaly", )"
                + R"("anomaly": {)"
                + R"("ip": ")" + _ips[index_anomalies(i)] + "\","
                + R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ","
                + R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ","
                + R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ","
                + R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ","
                + R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ","
                + R"("distance": )" + std::to_string(alerts(DISTANCE, i))
                + "}}\n");
        _log_file->Write(log_line);
    }

    return true;
};

bool AnomalyThreadManager::WriteRedis(const arma::uvec index_anomalies, const arma::mat alerts){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::WriteRedis:: Starting writing alerts in Redis...");
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        std::string log_line(R"({"time": ")" + darwin::time_utils::GetTime() + R"(", "filter": "tanomaly", )"
                + R"("anomaly": {)"
                + R"("ip": ")" + _ips[index_anomalies(i)] + "\","
                + R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ","
                + R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ","
                + R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ","
                + R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ","
                + R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ","
                + R"("distance": )" + std::to_string(alerts(DISTANCE, i))
                + "}}\n");

        if(not _redis_alerts_list.empty()) {
            DARWIN_LOG_DEBUG("AnomalyThread::WriteRedis:: Writing to Redis list: " + _redis_alerts_list);
            if(redis.Query(std::vector<std::string>{"LPUSH", _redis_alerts_list, log_line}) == REDIS_REPLY_ERROR) {
                DARWIN_LOG_WARNING("AnomalyThreadManager::REDISAddLogs:: Failed to add log in Redis !");
                return false;
            }
        }

        if(not _redis_alerts_channel.empty()) {
            DARWIN_LOG_DEBUG("AnomalyThread::WriteRedis:: Writing to Redis channel" + _redis_alerts_channel);
            if(redis.Query(std::vector<std::string>{"PUBLISH", _redis_alerts_channel, log_line}) == REDIS_REPLY_ERROR) {
                DARWIN_LOG_WARNING("AnomalyThreadManager::REDISAddLogs:: Failed to publish log in Redis !");
                return false;
            }
        }
    }



    return true;
}

void AnomalyThreadManager::PreProcess(std::vector<std::string> logs){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::PreProcess:: Starting the pre-process...");

    size_t size, pos, i;
    char delimiter = ';';
    std::array<int, 5> values{};
    std::string ip, protocol, port;

    // data to be pre-processed : {
    //                                  ip1 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ip2 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ...
    //                            }
    tsl::hopscotch_map<std::string, std::array<int, 5>> data;
    tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> cache_data;

    for (std::string l: logs){
        // line of log : ip_src;ip_dst;port;(udp|tcp) or  ip_src;ip_dst;(icmp)
        // where udp, tcp and icmp are represented by the ip protocol number
        try{
            size = l.size();

            pos = l.find(delimiter);
            if (pos==std::string::npos){
                DARWIN_LOG_WARNING("AnomalyThread::PreProcess:: Error when parsing a log line, line ignored");
                continue;
            }
            ip = l.substr(0, pos);
            l.erase(0, pos+1);

            pos = l.find_last_of(delimiter);
            if (pos==std::string::npos){
                DARWIN_LOG_WARNING("AnomalyThread::PreProcess:: Error when parsing a log line, line ignored");
                continue;
            }
            protocol = l.substr(pos+1);
            l.erase(pos, size);

            if(protocol=="1"){
                PreProcessLine(ip, protocol, "", cache_data, data);

            }else{
                pos = l.find_last_of(delimiter);
                if (pos==std::string::npos){
                    DARWIN_LOG_WARNING("AnomalyThread::PreProcess:: Error when parsing a log line, line ignored");
                    continue;
                }
                port = l.substr(pos+1, size);
                PreProcessLine(ip, protocol, port, cache_data, data);

            }

        }catch (const std::out_of_range& e){
            DARWIN_LOG_WARNING("AnomalyThread::PreProcess:: Error when parsing a log line, line ignored");
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

void AnomalyThreadManager::PreProcessLine(const std::string& ip, const std::string& protocol, std::string port,
                                          tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_data,
                                          tsl::hopscotch_map<std::string, std::array<int, 5>>& data){

    tsl::hopscotch_set<std::string>* set;
    if(data.find(ip) != data.end()){

        if (protocol=="1"){
            data[ip][ICMP_NB_HOST] += 1;

        }else if (protocol=="17"){
            data[ip][UDP_NB_HOST] += 1;
            set = &cache_data[ip+":17"];

            if(set->find(port) == set->end()) {
                data[ip][UDP_NB_PORT] += 1;
                set->emplace(port);
            }

        }else if (protocol=="6"){
            data[ip][TCP_NB_HOST] += 1;
            set = &cache_data[ip+":6"];

            if(set->find(port) == set->end()) {
                data[ip][TCP_NB_PORT] += 1;
                set->emplace(port);
            }
        }
        return;
    }


    if (protocol=="1"){
        data.insert({ip, {0,0,0,0,1}});

    }else if (protocol=="17"){
        data.insert({ip, {1,1,0,0,0}});
        cache_data[ip+":17"].emplace(port);

    }else if (protocol=="6"){
        data.insert({ip, {0,0,1,1,0}});
        cache_data[ip+":6"].emplace(port);
    }
}


long long int AnomalyThreadManager::REDISListLen() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISListLen:: Querying Redis for list size...");

    long long int result;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    if(redis.Query(std::vector<std::string>{"SCARD", _redis_internal}, result) != REDIS_REPLY_INTEGER) {
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

    if(redis.Query(std::vector<std::string>{"SPOP", _redis_internal, std::to_string(len)}, result) != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISPopLogs:: Not the expected Redis response");
        return -1;
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

    if(redis.Query(arguments) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISReinsertLogs:: Not the expected Redis response");
    }

    DARWIN_LOG_DEBUG("AnomalyThread::REDISReinsertLogs:: Reinsertion done");
    return true;
}
