/// \file     fAnomalyConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <iostream>

#include "fAnomalyConnector.hpp"

fAnomalyConnector::fAnomalyConnector(std::string filter_socket_path, int interval, std::string redis_list) : 
                    AConnector(ANOMALY, filter_socket_path, interval, redis_list) {}

bool fAnomalyConnector::sendData(std::map<std::string, std::string> input_line) {
    _input_line = input_line;
    _entry.clear();

    // TODO Boucler sur un tableau à la place
    if (not this->parseData("net_src_ip"))
        return false;
    if (not this->parseData("nb_dst_port_udp"))
        return false;
    if (not this->parseData("nb_host_udp"))
        return false;
    if (not this->parseData("nb_dst_port_tcp"))
        return false;
    if (not this->parseData("nb_host_tcp"))
        return false;
    if (not this->parseData("nb_host_icmp"))
        return false;

    static int i = 0; // TODO a remove, juste pour que toutes les entrées ne soient pas identiques pendant la période de dev
    this->_entry += ";\"" + std::to_string(i++) + "\"";

    this->REDISAddEntry();
    return true;
}