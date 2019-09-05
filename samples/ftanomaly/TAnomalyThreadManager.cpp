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

AnomalyThreadManager::AnomalyThreadManager(std::shared_ptr<darwin::toolkit::RedisManager> redis_manager,
                                           std::string log_file_path,
                                           std::string redis_list_name)
        : _log_file_path{std::move(log_file_path)},_redis_manager{std::move(redis_manager)},
          _redis_list_name{std::move(redis_list_name)}{}

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

    WriteLogs(index_anomalies, alerts);
    _matrix.reset();
}

bool AnomalyThreadManager::WriteLogs(arma::uvec index_anomalies, arma::mat alerts){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::WriteAlerts:: Starting writing alerts in logs file : \""
                     +_log_file_path+"\"...");

    std::ofstream logFile(_log_file_path, std::ios_base::app);
    if (!logFile.is_open()) {
        DARWIN_LOG_ERROR("AnomalyThread::WriteAlerts:: Error when opening the logs file, "
                         "maybe too low space disk or wrong permission");
        return false;
    }

    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        logFile << R"({"time": ")" + darwin::time_utils::GetTime() + R"(", "anomaly": {)"
                << R"("ip": ")" + _ips[index_anomalies(i)] + "\","
                << R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ","
                << R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ","
                << R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ","
                << R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ","
                << R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ","
                << R"("distance": )" + std::to_string(alerts(DISTANCE, i))
                << "}}"
                << endl;
    }
    logFile.close();

    return true;
};

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

    redisReply *reply = nullptr;
    long long int result;

    std::vector<std::string> arguments;
    arguments.emplace_back("SCARD");
    arguments.emplace_back(_redis_list_name);

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        freeReplyObject(reply);
        DARWIN_LOG_ERROR("AnomalyThread::REDISListLen:: Something went wrong while querying Redis");
        return -1;
    }

    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        freeReplyObject(reply);
        DARWIN_LOG_ERROR("AnomalyThread::REDISListLen:: Not the expected Redis response ");
        return -1;
    }

    result = reply->integer;

    freeReplyObject(reply);
    reply = nullptr;

    return result;
}

bool AnomalyThreadManager::REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISPopLogs:: Querying Redis for logs...");

    redisReply *reply = nullptr;
    unsigned int j;

    std::vector<std::string> arguments;
    arguments.emplace_back("SPOP");
    arguments.emplace_back(_redis_list_name);
    arguments.emplace_back(std::to_string(len));

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISPopLogs:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        return -1;
    }

    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISPopLogs:: Not the expected Redis response ");
        freeReplyObject(reply);
        return -1;
    }

    for (j=0; j<reply->elements; j++) {
        logs.emplace_back(reply->element[j]->str);
    }

    freeReplyObject(reply);
    reply = nullptr;

    return true;
}

bool AnomalyThreadManager::REDISReinsertLogs(std::vector<std::string> &logs) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::REDISReinsertLogs:: Querying Redis to reinsert logs...");

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(_redis_list_name);

    for (std::string& log: logs){
        arguments.emplace_back(log);
    }

    redisReply *reply = nullptr;

    if (!_redis_manager->REDISQuery(&reply, arguments)) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISReinsertLogs:: Something went wrong while querying Redis");
        freeReplyObject(reply);
        return false;
    }

    if (!reply || reply->type != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("AnomalyThread::REDISReinsertLogs:: Not the expected Redis response");
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;

    DARWIN_LOG_DEBUG("AnomalyThread::REDISReinsertLogs:: Reinsertion done");
    return true;
}
