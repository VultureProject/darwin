/// \file     PCRTask.cpp
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
#include "PCRTask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

PCRTask::PCRTask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 std::mutex& cache_mutex)
        : Session{DARWIN_FILTER_NAME, socket, manager, cache, cache_mutex}
    {
    _is_cache = _cache != nullptr;
}

long PCRTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_EXFILDATA;
}

void PCRTask::operator()() {
    DARWIN_LOGGER;

    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = _body.GetArray();
    rapidjson::Document output;
    rapidjson::Value& output_array = output.SetArray();
    output_array.Reserve(array.Size(), output.GetAllocator());

    STAT_INPUT_INC;
    SetStartingTime();
    for (rapidjson::Value &value : array) {
        // We have a generic hash function, which takes no arguments as these can be of very different types depending
        // on the nature of the filter
        // So instead, we set an attribute corresponding to the current domain being processed, to compute the hash
        // accordingly

        if(ParseLine(value)) {

            double sent = static_cast<double>(bytes_sent);
            double received = static_cast<double>(bytes_received);
            double pcr = 0.0f;
            if(sent + received == 0.0f){
                DARWIN_LOG_WARNING("PCRTask:: PCR set to 0 because no bytes were sent or received in this time interval");
                pcr = 0.0f;
            } else {
                pcr = (sent - received) / (sent + received);
            }

            output_array.PushBack(pcr, output.GetAllocator());
        }
        else {
            STAT_PARSE_ERROR_INC;
            DARWIN_LOG_WARNING("PCRTask:: PCR set to 0 because there was an error persing a data");
            output_array.PushBack(0.0f, output.GetAllocator());
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    output.Accept(writer);

    _response_body.clear();
    _response_body.append( buffer.GetString());
}

PCRTask::~PCRTask() = default;



bool PCRTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("PCRTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if(values.Size() != 2) {
        DARWIN_LOG_ERROR("PCRTask:: ParseBody: The input line misses a member 'aggregations'");
        return false;
    }

    if(not values[0].IsUint64() || not values[1].IsUint64()){
        DARWIN_LOG_ERROR("PCRTask:: ParseBody: The input line misses a member 'aggregations'");
        return false;
    }

    bytes_received = values[0].GetUint64();
    bytes_sent = values[1].GetUint64();

    return true;
}
