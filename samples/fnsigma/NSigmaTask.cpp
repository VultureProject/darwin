/// \file     NSigmaTask.cpp
/// \authors  Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \version  1.0
/// \date     18/08/2021
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>
#include <armadillo>
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/Validators.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../toolkit/rapidjson/document.h"
#include "NSigmaTask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

NSigmaTask::NSigmaTask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 std::mutex& cache_mutex, int n_sigma)
        : Session{DARWIN_FILTER_NAME, socket, manager, cache, cache_mutex}, _n_sigma{n_sigma}
    {
    _is_cache = _cache != nullptr;
}

long NSigmaTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_EXFILDATA;
}

void NSigmaTask::operator()() {
    DARWIN_LOGGER;

    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = _body.GetArray();

    STAT_INPUT_INC;
    SetStartingTime();
    for (rapidjson::Value &value : array) {

        if(not ParseLine(value))  {
            STAT_PARSE_ERROR_INC;
            DARWIN_LOG_ERROR("NSigmaTask:: there has been an error parsing some data, alerting this but resuming execution");
            DARWIN_ALERT_MANAGER.Alert("Parsing error, expecting array of numbers", 101, Evt_idToString());
        }
    }
    if(input_pcr.size() < MINIMAL_SIZE){
        DARWIN_LOG_ERROR("NSigmaTask:: No data was given, alerting this");
        DARWIN_ALERT_MANAGER.Alert("No data", 101, Evt_idToString());
        return;
    }
    auto pcrs = arma::rowvec(input_pcr);
    double pcr_sigma = arma::stddev(pcrs);
    double pcr_mean = arma::mean(pcrs);
    double suspicion_threshold = pcr_mean + _n_sigma*pcr_sigma;
    DARWIN_LOG_DEBUG("NSigmaTask:: (): for " + std::to_string(pcrs.size()) + " values, mean: " 
            + std::to_string(pcr_mean) + "; sigma: " + std::to_string(pcr_sigma) + "; thrshold: " + std::to_string(suspicion_threshold));
    
    for(size_t i =0; i< input_pcr.size(); i++){
        double pcr = input_pcr[i];
        if(pcr < suspicion_threshold){
            filtered_pcr.push_back(pcr);
        } else{
            filtered_indices.push_back(i);
        }
    }
    
    rapidjson::Document output;
    rapidjson::Value& output_array = output.SetArray();
    output_array.Reserve(filtered_pcr.size(), output.GetAllocator());

    for(double p:filtered_pcr) {
        output_array.PushBack(p, output.GetAllocator());
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    output.Accept(writer);

    _response_body.clear();
    _response_body.append( buffer.GetString());

}

NSigmaTask::~NSigmaTask() = default;



bool NSigmaTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsDouble()) {
        DARWIN_LOG_ERROR("NSigmaTask:: ParseBody: The input line is not an object");
        return false;
    }

    input_pcr.push_back(line.GetDouble());
    return true;
}
