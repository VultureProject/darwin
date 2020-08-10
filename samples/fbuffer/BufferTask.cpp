/// \file     BufferTask.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     22/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/tokenizer.hpp>
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
                 std::vector<std::pair<std::string, valueType>> &inputs,
                 std::vector<std::shared_ptr<AConnector>> &connectors)
        : Session{"buffer", socket, manager, cache, cache_mutex},
            _inputs_format(inputs),
            _connectors(connectors) {
}

long BufferTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_BUFFER;
}

void BufferTask::operator()() {
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;
    std::string body;

    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = this->_body.GetArray();

    SetStartingTime();
    for (rapidjson::Value &line : array) {
        STAT_INPUT_INC;
        if(ParseLine(line)) {
            STAT_MATCH_INC;
            if (is_log) {
                _logs += _data + "\n";
            }
        } else {
            STAT_PARSE_ERROR_INC;
        }
    }
}

bool BufferTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("BufferTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != _inputs_format.size() + 1) { // +1 for source filed in input always present so not in config file
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: You must provide as many elements in the list as number of inputs in config");
        return false;
    }

    this->_input_line.clear();

    int i = -1;
    for (auto &value : values) {
        if (i == -1) {
            if (not value.IsString()) {
                DARWIN_LOG_ERROR("BufferTask::ParseLine:: the source given must be a string.");
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

    DARWIN_LOG_DEBUG("BufferTask:: processed entry in " + std::to_string(GetDurationMs()) + "ms");
    std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
            R"(", "filter": ")" + GetFilterName() + "\", \"content\": \""+ "TOTOTOTO" + "\"}";
    this->AddEntries();

    return true;
}

bool BufferTask::ParseData(rapidjson::Value &data, valueType input_type) {
    DARWIN_LOGGER;

    switch (input_type) {
        case STRING:
            if (not data.IsString()) {
                DARWIN_LOG_ERROR("BufferTask:: ParseData: The value sent must be a string");
                return false;
            }
            this->_data = data.GetString();
            break;
        case INT:
            if (not data.IsInt()) {
                DARWIN_LOG_ERROR("BufferTask:: ParseData: The value sent must be an int");
                return false;
            }
            this->_data = std::to_string(data.GetInt64());
            break;
        default:
            return false;
    }

    DARWIN_LOG_DEBUG("BufferTask:: ParseData: Parsed element: " + _data);

    return true;
}

bool BufferTask::AddEntries() {
    DARWIN_LOGGER;
    bool error = false;

    for (auto &connector : this->_connectors) {
        if (not connector->sendToRedis(this->_input_line)) {
            error = true;
        }
    }
    return not error;
}