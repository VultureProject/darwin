/// \file     fAnomalyConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     02/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include <iostream>

#include "../../../toolkit/StringUtils.hpp"
#include "fAnomalyConnector.hpp"

fAnomalyConnector::fAnomalyConnector(boost::asio::io_context &context, std::string &filter_socket_path, unsigned int interval, std::vector<std::pair<std::string, std::string>> &redis_lists, unsigned int required_log_lines) : 
                    AConnector(context, ANOMALY, filter_socket_path, interval, redis_lists, required_log_lines) {}

bool fAnomalyConnector::FormatDataToSendToFilter(std::vector<std::string> &logs, std::string &res) {
    tsl::hopscotch_map<std::string, std::array<int, 5>> data = this->PreProcess(logs);

    if (data.size() < 6) { // TODO Change for 10
        // TODO rajouter un log
        return false;
    }

    std::string subres;

    res = "[[";
    int i = 0;

    for (const auto &item : data) {
        subres = item.first;

        for (int i : item.second) {
            subres += ", " + std::to_string(i);
        }
        
        if (i != 0)
            res += "], [";
        res += subres;
        i++;
    }
    res += "]]";
    return true;
}

bool fAnomalyConnector::sendToRedis(std::map<std::string, std::string> &input_line) {
    this->_input_line = input_line;
    this->_entry.clear();

    std::string source = this->getSource(input_line);

    if (not this->parseData("net_src_ip", STRING))
        return false;
    if (not this->parseData("net_dst_ip", STRING))
        return false;
    if (not this->parseData("net_dst_port", STRING))
        return false;
    if (not this->parseData("ip_proto", STRING))
        return false;

    for (const auto &redis_config : this->_redis_lists) {
        if (not redis_config.first.compare(source) or redis_config.first.empty())
                this->REDISAddEntry(this->_entry, redis_config.second);
    }
    return true;
}

tsl::hopscotch_map<std::string, std::array<int, 5>> fAnomalyConnector::PreProcess(const std::vector<std::string> &logs) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("AnomalyThread::PreProcess:: Starting the pre-process...");

    size_t size, pos, i;
    char delimiter = ';';
    std::array<int, 5> values{};
    std::string ip, ip_dst, port, protocol;
    std::vector<std::string> vec;

    // data to be pre-processed : {
    //                                  ip1 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ip2 : [net_src_ip4, udp_nb_host, udp_nb_port, tcp_nb_host, tcp_nb_port, icmp_nb_host],
    //                                  ...
    //                            }
    tsl::hopscotch_map<std::string, std::array<int, 5>> data;
    tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> cache_port;
    tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> cache_ip;

    for (const std::string &l : logs) {
        // line of log : ip_src;ip_dst;port;(udp|tcp) or  ip_src;ip_dst;(icmp)
        // where udp, tcp and icmp are represented by the ip protocol number
        try{
            vec = SplitString(l, delimiter);

            if (vec.size() != 4) {
                DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (element missing), line ignored");
                continue;
            }
            ip = vec[0];
            ip_dst = vec[1];
            port = vec[2];
            protocol = vec[3];

            if (ip.empty() or ip_dst.empty() or protocol.empty() or (protocol != "1" and port.empty())) {
                DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (element empty), line ignored");
                continue;
            }

            PreProcessLine(ip, ip_dst, protocol, port, cache_port, cache_ip, data);

////// The following section is od and will disapear ///////////
            // size = l.size();

            // // Get ip_src
            // pos = l.find(delimiter);
            // if (pos==std::string::npos){
            //     DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (ip_src), line ignored");
            //     continue;
            // }
            // ip = l.substr(0, pos);
            // l.erase(0, pos+1);

            // // Get proto
            // pos = l.find_last_of(delimiter);
            // if (pos==std::string::npos){
            //     DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (protocole), line ignored");
            //     continue;
            // }
            // protocol = l.substr(pos+1);
            // l.erase(pos, size);

            // // Get ip_dst
            // pos = l.find(delimiter);
            // if (pos == std::string::npos) {
            //     DARWIN_LOG_WARNING("fAnomalyConnector::PreProcess:: Error when parsing a log line (ip_dst), line ignored");
            //     continue;
            // }
            // ip_dst = l.substr(0, pos);
            // l.erase(0, pos + 1);

            // if (protocol=="1") {
            //     PreProcessLine(ip, ip_dst, protocol, "", cache_port, cache_ip, data);

            // } else {
            //     port = l;
            //     PreProcessLine(ip, ip_dst, protocol, port, cache_port, cache_ip, data);

            // }

        } catch (const std::out_of_range& e) {
            std::string warning("fAnomalyConnector::PreProcess:: Error when parsing a log line , line ignored: ");
            warning += e.what();
            DARWIN_LOG_WARNING(warning);
            continue;
        }
    }
    return data;
}

void fAnomalyConnector::PreProcessLine(const std::string& ip, const std::string &ip_dst, const std::string& protocol, std::string port,
                                          tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_port,
                                          tsl::hopscotch_map<std::string, tsl::hopscotch_set<std::string>> &cache_ip,
                                          tsl::hopscotch_map<std::string, std::array<int, 5>>& data) {
    tsl::hopscotch_set<std::string>* set_ip;
    tsl::hopscotch_set<std::string>* set_port;

    if(data.find(ip) != data.end()){

        if (protocol=="1") {
            set_ip = &cache_ip[ip + ":1"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][ICMP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }

        } else if (protocol=="17") {
            set_ip = &cache_ip[ip + ":17"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][UDP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }

            set_port = &cache_port[ip+":17"];
            if(set_port->find(port) == set_port->end()) {
                data[ip][UDP_NB_PORT] += 1;
                set_port->emplace(port);
            }

        } else if (protocol=="6") {
            set_ip = &cache_ip[ip + ":6"];
            if (set_ip->find(ip_dst) == set_ip->end()) {
                data[ip][TCP_NB_HOST] += 1;
                set_ip->emplace(ip_dst);
            }
    
            set_port = &cache_port[ip + ":6"];
            if(set_port->find(port) == set_port->end()) {
                data[ip][TCP_NB_PORT] += 1;
                set_port->emplace(port);
            }
        }
        return;
    }


    if (protocol=="1") {
        cache_ip[ip + ":1"].emplace(ip_dst);
        data.insert({ip, {0,0,0,0,1}});

    } else if (protocol=="17") {
        data.insert({ip, {1,1,0,0,0}});
        cache_ip[ip + ":17"].emplace(ip_dst);
        cache_port[ip+":17"].emplace(port);

    } else if (protocol=="6") {
        data.insert({ip, {0,0,1,1,0}});
        cache_ip[ip + ":6"].emplace(ip_dst);
        cache_port[ip+":6"].emplace(port);
    }
}
