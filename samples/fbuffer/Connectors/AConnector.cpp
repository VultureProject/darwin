/// \file     BufferTask.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     29/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "../../toolkit/RedisManager.hpp"
#include "AConnector.hpp"

AConnector::AConnector(outputType filter_type, std::string filter_socket_path, int interval, std::string redis_list, unsigned int minLogLen) :
                        _filter_type(std::move(filter_type)),
                        _filter_socket_path(std::move(filter_socket_path)),
                        _interval(interval),
                        _redis_list(redis_list),
                        _minLogLen(minLogLen) {}

int AConnector::getInterval() const {
    return this->_interval;
}

std::string AConnector::getRedisList() const {
    return this->_redis_list;
}

unsigned int AConnector::getRequiredLogLength() const {
    return this->_minLogLen;
}

bool AConnector::parseData(std::string fieldname) {
    DARWIN_LOGGER;
    if (_input_line.find(fieldname) == _input_line.end()) {
        DARWIN_LOG_ERROR("AConnector::parseData '" + fieldname + "' is missing in the input line. Output ignored.");
        return false;
    }
    if (not this->_entry.empty()) {
        this->_entry += ";";
    }
    this->_entry += "\"" + _input_line[fieldname] + "\"";
    return true;
}
 
bool AConnector::REDISAddEntry() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AConnector::REDISAddEntry:: Add data in Redis...");

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();

    std::vector<std::string> arguments;
    arguments.emplace_back("SADD");
    arguments.emplace_back(this->_redis_list);
    arguments.emplace_back(this->_entry);

    if(redis.Query(arguments, true) != REDIS_REPLY_INTEGER) {
        DARWIN_LOG_ERROR("BufferTask::REDISAdd:: Not the expected Redis response");
        return false;
    }
    return true;
}
