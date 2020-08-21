/// \file     fSofaConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "fSofaConnector.hpp"

fSofaConnector::fSofaConnector(boost::asio::io_context &context, std::string &filter_socket_path, unsigned int interval, std::vector<std::pair<std::string, std::string>> &redis_lists, unsigned int minLogLen) : 
                    AConnector(context, darwin::SOFA, filter_socket_path, interval, redis_lists, minLogLen) {}

bool fSofaConnector::ParseInputForRedis(std::map<std::string, std::string> &input_line) {
    this->_input_line = input_line;
    this->_entry.clear();

    std::string source = this->GetSource();

    if (not this->ParseData("ip"))
        return false;
    if (not this->ParseData("hostname"))
        return false;
    if (not this->ParseData("os"))
        return false;
    if (not this->ParseData("proto"))
        return false;
    if (not this->ParseData("port"))
        return false;

    for (const auto &redis_config : this->_redis_lists) {
        // If the source in the input is equal to the source in the redis list, or the redis list's source is ""
        if (not redis_config.first.compare(source) or redis_config.first.empty())
                this->REDISAddEntry(this->_entry, redis_config.second);
    }
    return true;
}