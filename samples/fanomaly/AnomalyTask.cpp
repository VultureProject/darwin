/// \file     AnomalyTask.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <thread>
#include <string>
#include <regex>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/lru_cache.hpp"
#include "AnomalyTask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "ASession.hpp"
#include "AlertManager.hpp"

AnomalyTask::AnomalyTask(std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               darwin::session_ptr_t s,
                               darwin::DarwinPacket& packet)
        : ATask(DARWIN_FILTER_NAME, cache, cache_mutex, s, packet){}

void AnomalyTask::operator()() {
    DARWIN_LOGGER;
    SetStartingTime();

    // DARWIN_LOG_DEBUG("AnomalyTask:: body: " + JsonStringify(_body));

    // Should not fail, as the Session body parser MUST check for validity !
    auto array = _body.GetArray();

    for (auto &value : array)
    {
        STAT_INPUT_INC;
        if(ParseLine(value)) {
            Detection();
        }
        else {
            STAT_PARSE_ERROR_INC;
            _packet.AddCertitude(DARWIN_ERROR_RETURN);
        }
    }

    DARWIN_LOG_DEBUG("AnomalyTask:: processed task in " + std::to_string(GetDurationMs()));
}

long AnomalyTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_ANOMALY;
}

bool AnomalyTask::Detection(){
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask::Detection:: Start detection ...");
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
        DARWIN_LOG_DEBUG("AnomalyTask::Detection:: No anomalies found");
        _packet.AddCertitude(0);
        return false;
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

    STAT_MATCH_INC;
    _packet.AddCertitude(100);

    GenerateAlerts(_ips, index_anomalies, alerts);
    if(_s->GetOutputType() == darwin::config::output_type::LOG){
        GenerateLogs(_ips, index_anomalies, alerts);
    }
    return true;
}

void AnomalyTask::GenerateAlerts(std::vector<std::string> ips, arma::uvec index_anomalies, arma::mat alerts){
    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        std::string details;
        details = R"({"ip": ")" + ips[index_anomalies(i)] + "\",";
        details += R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ",";
        details += R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ",";
        details += R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ",";
        details += R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ",";
        details += R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ",";
        details += R"("distance": )" + std::to_string(alerts(DISTANCE, i));
        details += "}";
        DARWIN_ALERT_MANAGER.Alert(ips[index_anomalies(i)], 100, _packet.Evt_idToString(), details);
    }
}

void AnomalyTask::GenerateLogs(std::vector<std::string> ips, arma::uvec index_anomalies, arma::mat alerts){
    auto logs = _packet.GetMutableLogs();
    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        std::string alert_log;
        alert_log = R"({"evt_id": ")" + _packet.Evt_idToString();
        alert_log += R"(", "time": ")" + darwin::time_utils::GetTime();
        alert_log += R"(", "filter": ")" + GetFilterName();
        alert_log += R"(", "anomaly": {)";
        alert_log += R"("ip": ")" + ips[index_anomalies(i)] + "\",";
        alert_log += R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ",";
        alert_log += R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ",";
        alert_log += R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ",";
        alert_log += R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ",";
        alert_log += R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ",";
        alert_log += R"("distance": )" + std::to_string(alerts(DISTANCE, i));
        alert_log += "}}";
        logs += alert_log + '\n';
    }
};

bool AnomalyTask::ParseLine(rapidjson::Value &cluster) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask::ParseLine:: parsing cluster");
    unsigned int i = 0;
    std::string ip;
    size_t size;

    if(not cluster.IsArray()) {
        DARWIN_LOG_ERROR("AnomalyTask:: ParseLine: The input line is not an array");
        return false;
    }

    size = cluster.Size();
    DARWIN_LOG_DEBUG("AnomalyTask::ParseLine:: cluster size: " + std::to_string(size));
    if (size <= MIN_DATA) {
        DARWIN_LOG_ERROR("AnomalyTask::ParseLine:: The list provided is not enough, "
                            "need at least " + std::to_string(MIN_DATA) + " data");
        return false;
    }
    _matrix.zeros(5, size);
    _ips.clear();

    auto requests = cluster.GetArray();
    for (auto &request: requests) {
        if (!request.IsArray()) {
            DARWIN_LOG_ERROR("AnomalyTask::ParseLine:: For each request, you must provide a list");
            return false;
        }

        auto data = request.GetArray();

        if (data.Size() != 6) {
            DARWIN_LOG_ERROR("AnomalyTask::ParseLine:: Each list must contain 6 elements");

            return false;
        }

        if (!data[0].IsString()) {
            DARWIN_LOG_ERROR("AnomalyTask:: ParseLine: The first element must be a String");
            return false;
        }

        ip = data[0].GetString();
        _ips.emplace_back(ip);

        for (unsigned int j = 1; j < 6; j++) {
            if (!data[j].IsInt()) {
                DARWIN_LOG_ERROR("AnomalyTask:: ParseLine: The non-first elements must be integers");
                return false;
            }
            _matrix(j - 1, i) = data[j].GetInt();
        }
        i++;
    }

    DARWIN_LOG_DEBUG("AnomalyTask:: matrix size: " + std::to_string(_matrix.size()));


    return true;
}
