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
                 std::shared_ptr<BufferThreadManager> vat,
                 std::vector<std::pair<std::string, valueType>> inputs,
                 std::vector<std::shared_ptr<AConnector>> connectors)
        : Session{"buffer", socket, manager, cache, cache_mutex},
            _buffer_thread_manager(std::move(vat)),
            _inputs_format(inputs),
            _connectors(connectors) {
}

long BufferTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_BUFFER;
}

void BufferTask::operator()() {
    DARWIN_LOGGER;
    bool is_log = GetOutputType() == darwin::config::output_type::LOG;
    std::string body;

    // Should not fail, as the Session body parser MUST check for validity !
    rapidjson::GenericArray<false, rapidjson::Value> array = this->_body.GetArray();

    // TODO refactor cette partie, Parseline doit prendre une ligne complète et la parser.
    unsigned int i = 0;
    this->_input_line.clear();
    SetStartingTime();
    for (rapidjson::Value &value : array) {
        STAT_INPUT_INC;
        if(ParseLine(value, this->_inputs_format[i])) {
            STAT_MATCH_INC;
            this->_input_line[this->_inputs_format[i].first] = this->_string;
            if (is_log) {
                _logs += _string + "\n";
            }
        } else {
            STAT_PARSE_ERROR_INC;
        }
        i++;
    }
    DARWIN_LOG_DEBUG("BufferTask:: processed entry in " + std::to_string(GetDurationMs()) + "ms");
    std::string alert_log = R"({"evt_id": ")" + Evt_idToString() + R"(", "time": ")" + darwin::time_utils::GetTime() +
            R"(", "filter": ")" + GetFilterName() + "\", \"content\": \""+ "TOTOTOTO" + "\"}";
    this->AddEntries();
}

BufferTask::~BufferTask() = default;

bool BufferTask::ParseLine(rapidjson::Value &line) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("BufferTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: You must provide only one string in the list");
        return false;
    }

    if (!values[0].IsString()) {
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: The string sent must be a string");
        return false;
    }

    this->_string = values[0].GetString();
    DARWIN_LOG_DEBUG("BufferTask:: ParseLine: Parsed element: " + _string);

    return true;
}

bool BufferTask::ParseLine(rapidjson::Value &line, std::pair<std::string, valueType> &input) {
    DARWIN_LOGGER;

    if(not line.IsArray()) {
        DARWIN_LOG_ERROR("BufferTask:: ParseBody: The input line is not an array");
        return false;
    }

    auto values = line.GetArray();

    if (values.Size() != 1) {
        DARWIN_LOG_ERROR("BufferTask:: ParseLine: You must provide only one string in the list");
        return false;
    }

    switch (input.second) {
        case STRING:
            if (!values[0].IsString()) {
                DARWIN_LOG_ERROR("BufferTask:: ParseLine: The value sent must be a string");
                return false;
            }
            this->_string = values[0].GetString();
            break;
        case INT:
            if (!values[0].IsInt()) {
                DARWIN_LOG_ERROR("BufferTask:: ParseLine: The value sent must be an int");
                return false;
            }
            this->_string = std::to_string(values[0].GetInt64());
            break;
        // TODO : rajouter les cas manquants et le default
    }

    DARWIN_LOG_DEBUG("BufferTask:: ParseLine: Parsed element: " + _string);

    return true;
}

bool BufferTask::AddEntries() {
    DARWIN_LOGGER;
    for (auto &connector : this->_connectors) {
        if (not connector->sendData(this->_input_line)) {
            DARWIN_LOG_WARNING("BufferTask::AddEntries: unable to send data to " + connector->getRedisList() + " redis list.");
        }
    }
    return true;
}