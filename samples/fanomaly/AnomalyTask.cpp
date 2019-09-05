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
#include "protocol.h"

AnomalyTask::AnomalyTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache)
        : Session{socket, manager, cache}{}

void AnomalyTask::operator()() {
    DARWIN_LOGGER;
    SetStartingTime();
    for (size_t i = 0; i < _matrixies.size(); ++i)
    {
        Detection(_matrixies[i], _ips[i]);
    }

    DARWIN_LOG_DEBUG("AnomalyTask:: processed task in " + std::to_string(GetDurationMs()));
    Workflow();
    _matrixies = std::vector<arma::mat>();
    _ips = std::vector<std::vector<std::string>>();
}

long AnomalyTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_ANOMALY;
}

void AnomalyTask::Workflow() {
    switch (header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendResToSession();
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
        default:
            break;
    }
}

bool AnomalyTask::Detection(arma::mat matrix, const std::vector<std::string> &ips){
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

    alerts = matrix;

    // reduce matrix' dimension from 5 to 2
    pcaa.Apply(matrix, newDataDimension);
    db.Cluster(matrix, assignments);

    no_anomalies = matrix;
    anomalies    = matrix;

    index_anomalies = arma::find(assignments == SIZE_MAX); // index of noises find by the clustering in the _matrix
    if (index_anomalies.is_empty()){
        DARWIN_LOG_DEBUG("AnomalyTask::Detection:: No anomalies found");
        _certitudes.push_back(0);
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

    _certitudes.push_back(100);

    if(GetOutputType() == darwin::config::output_type::LOG){
        GenerateLogs(ips, index_anomalies, alerts);
    }
    return true;
}

void AnomalyTask::GenerateLogs(std::vector<std::string> ips, arma::uvec index_anomalies, arma::mat alerts){

    for(unsigned int i=0; i<index_anomalies.n_rows; i++){
        _logs += R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() + R"(", "anomaly": {)";
        _logs += R"("ip": ")" + ips[index_anomalies(i)] + "\",";
        _logs += R"("udp_nb_host": )" + std::to_string(alerts(UDP_NB_HOST, i)) + ",";
        _logs += R"("udp_nb_port": )" + std::to_string(alerts(UDP_NB_PORT, i)) + ",";
        _logs += R"("tcp_nb_host": )" + std::to_string(alerts(TCP_NB_HOST, i)) + ",";
        _logs += R"("tcp_nb_port": )" + std::to_string(alerts(TCP_NB_PORT, i)) + ",";
        _logs += R"("icmp_nb_host": )" + std::to_string(alerts(ICMP_NB_HOST, i)) + ",";
        _logs += R"("distance": )" + std::to_string(alerts(DISTANCE, i));
        _logs += "}}\n";
    }
};

bool AnomalyTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyTask::ParseBody:: " + body);
    unsigned int i;
    std::string ip;
    size_t size;

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("AnomalyTask::ParseBody:: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        for (auto &cluster: values) {
            std::vector<std::string> ips;
            arma::mat matrix;
            i = 0;

            size = cluster.Size();
            if (size <= MIN_DATA) {
                DARWIN_LOG_ERROR("AnomalyTask::ParseBody:: The list provided is not enough, "
                                 "need at least " + std::to_string(MIN_DATA) + " data");
                return false;
            }
            matrix.zeros(5, size);

            auto requests = cluster.GetArray();
            for (auto &request: requests) {
                if (!request.IsArray()) {
                    DARWIN_LOG_ERROR("AnomalyTask::ParseBody:: For each request, you must provide a list");
                    return false;
                }

                auto data = request.GetArray();

                if (data.Size() != 6) {
                    DARWIN_LOG_ERROR("AnomalyTask::ParseBody:: Each list must contain 6 elements");

                    return false;
                }

                if (!data[0].IsString()) {
                    DARWIN_LOG_ERROR("AnomalyTask:: ParseBody: The first element must be a String");
                    return false;
                }

                ip = data[0].GetString();
                ips.emplace_back(ip);

                for (unsigned int j = 1; j < 6; j++) {
                    if (!data[j].IsInt()) {
                        DARWIN_LOG_ERROR("AnomalyTask:: ParseBody: The non-first elements must be integers");
                        return false;
                    }
                    matrix(j - 1, i) = data[j].GetInt();
                }
                i++;
            }
            _matrixies.emplace_back(matrix);
            _ips.emplace_back(ips);
        }

    } catch (...) {
        DARWIN_LOG_CRITICAL("AnomalyTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
