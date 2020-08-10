/// \file     fBufferConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <iostream>

#include "fBufferConnector.hpp"

fBufferConnector::fBufferConnector(boost::asio::io_context &context, std::string &filter_socket_path, unsigned int interval, std::vector<std::pair<std::string, std::string>> &redis_lists, unsigned int minLogLen) : 
                    AConnector(context, BUFFER, filter_socket_path, interval, redis_lists, minLogLen) {}

bool fBufferConnector::sendToRedis(std::map<std::string, std::string> &input_line) {
    _input_line = input_line;
    _entry.clear();

    std::string source = this->getSource(input_line);

    if (not this->parseData("net_src_ip", STRING))
        return false;
    if (not this->parseData("net_dst_ip", STRING))
        return false;
    if (not this->parseData("net_dst_port", STRING))
        return false;
    if (not this->parseData("ip_proto", STRING))
        return false;
    if (not this->parseData("ip", STRING))
        return false;
    if (not this->parseData("hostname", STRING))
        return false;
    if (not this->parseData("os", STRING))
        return false;
    if (not this->parseData("proto", STRING))
        return false;
    if (not this->parseData("port", STRING))
        return false;
    if (not this->parseData("test_int", INT))
        return false;

    for (const auto &redis_config : this->_redis_lists) {
        if (not redis_config.first.compare(source) or redis_config.first.empty())
                this->REDISAddEntry(this->_entry, redis_config.second);
    }
    return true;
}