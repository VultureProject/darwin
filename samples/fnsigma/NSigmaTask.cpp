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
                 std::mutex& cache_mutex, int n_sigma, size_t min_size)
	: Session{DARWIN_FILTER_NAME, socket, manager, cache, cache_mutex}, _n_sigma{n_sigma}, _minimum_size{min_size}
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
    if(input_values.size() < _minimum_size){
        DARWIN_LOG_ERROR("NSigmaTask:: No data was given, alerting this");
        DARWIN_ALERT_MANAGER.Alert("No data", 101, Evt_idToString());
        return;
    }
    auto values = arma::rowvec(input_values);
    DARWIN_LOG_DEBUG("NSigmaTask:: (): nb values " + std::to_string(values.size()) + ", min: " + std::to_string(_minimum_size)
            + "vec: " + std::to_string(input_values.size()));
    double sigma = arma::stddev(values);
    double mean = arma::mean(values);
    double suspicion_threshold = mean + _n_sigma*sigma;
    DARWIN_LOG_DEBUG("NSigmaTask:: (): for " + std::to_string(values.size()) + " values, mean: " 
            + std::to_string(mean) + "; sigma: " + std::to_string(sigma) + "; thrshold: " + std::to_string(suspicion_threshold));
    
    for(size_t i =0; i< input_values.size(); i++){
        double val = input_values[i];
        if(val < suspicion_threshold){
            filtered_values.push_back(val);
        } else{
            filtered_indices.push_back(i);
        }
    }
    
    rapidjson::Document output;
    rapidjson::Value& output_array = output.SetArray();
    output_array.Reserve(filtered_values.size(), output.GetAllocator());

    for(double p:filtered_values) {
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

    if(not line.IsNumber()) {
        DARWIN_LOG_ERROR("NSigmaTask:: ParseBody: The input line is not a number");
        return false;
    }

    // We convert to a double here since those should be relatively small numbers
    // In the case of very large or if a great precision is needed, this may lead to
    // a loss of precision (if we evalute numbers which *require* more than 52 significant bits)
    input_values.push_back(line.GetDouble());
    return true;
}
