/// \file     BufferTask.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <string>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/Validators.hpp"
#include "../toolkit/rapidjson/document.h"
#include "BufferTask.hpp"
#include "Logger.hpp"
#include "Stats.hpp"
#include "protocol.h"
#include "AlertManager.hpp"

BufferTask::BufferTask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 std::mutex& cache_mutex,
                 std::vector<std::pair<std::string, darwin::valueType>> &inputs,
                 std::vector<std::shared_ptr<AConnector>> &connectors)
        : Session{"buffer", socket, manager, cache, cache_mutex},
            _inputs_format(inputs),
            _connectors(connectors) {
}

long BufferTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_BUFFER;
}

void BufferTask::operator()() {
    DARWIN_LOGGER;
    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = this->_body.GetArray();

    SetStartingTime();
    for (rapidjson::Value &line : array) {
        STAT_INPUT_INC;
        if (ParseLine(line)) {
            this->_certitudes.push_back(0);
            this->AddEntries();
        } else {
            STAT_PARSE_ERROR_INC;
            this->_certitudes.push_back(DARWIN_ERROR_RETURN);
        }
        DARWIN_LOG_DEBUG("BufferTask:: processed entry in " + std::to_string(GetDurationMs()) + "ms");
    }
}

bool BufferTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != _inputs_format.size() + 1) { // +1 for source field in input always present so not in config file
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: You must provide a source (or an empty string) and as many elements in the list as number of inputs in your configuration");
        return false;
    }

    this->_input_line.clear();

    int i = -1; // Starting at -1 to handle the offset between input and output indexes due to source (1st element on the input)
    for (auto &value : values) {
        if (i == -1) {
            if (not value.IsString()) {
                DARWIN_LOG_ERROR("BufferTask::ParseLine:: the source given must be a string.");
                return false;
            }
            this->_input_line["source"] = value.GetString();
            i++;
            continue; //Skip the first value as it is the source.
        }
        if (not ParseData(value, _inputs_format[i].second))
            return false;
        
        this->_input_line[this->_inputs_format[i].first] = this->_data;
        i++;
    }
    return true;
}

bool BufferTask::ParseData(rapidjson::Value &data, darwin::valueType input_type) {
    DARWIN_LOGGER;

    switch (input_type) {
        case darwin::STRING:
            if (not data.IsString()) {
                DARWIN_LOG_ERROR("BufferTask::ParseData: The value sent must be a string");
                return false;
            }
            this->_data = data.GetString();
            break;
        case darwin::INT:
            if (not data.IsInt()) {
                DARWIN_LOG_ERROR("BufferTask::ParseData: The value sent must be an int");
                return false;
            }
            this->_data = std::to_string(data.GetInt64());
            break;
        case darwin::FLOAT:
            if (not data.IsDouble() and not data.IsInt64()) {
                DARWIN_LOG_ERROR("BufferTask::ParseData: The value sent must be a decimal");
                return false;
            }
            this->_data = std::to_string(data.GetDouble());
            break;
        default:
            return false;
    }

    DARWIN_LOG_DEBUG("BufferTask::ParseData: Parsed element: " + _data);

    return true;
}

bool BufferTask::AddEntries() {
    bool error = false;

    for (auto &connector : this->_connectors) {
        if (not connector->ParseInputForRedis(this->_input_line)) {
            error = true;
        }
    }
    return not error;
}
