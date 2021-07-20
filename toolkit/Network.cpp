/// \file     DecisionTask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     17/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.


extern "C" {

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

}


#include <cerrno>
#include <cstring>
#include <iostream>
#include <boost/asio/ip/address.hpp>
#include "Logger.hpp"
#include "StringUtils.hpp"

#include "Network.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace network
    namespace network {
        bool GetIpAddressType(const std::string &ip_address_string, int *ip_type) {
            DARWIN_LOGGER;

            struct addrinfo hints, *result;

            memset(&hints, 0, sizeof(hints));

            hints.ai_family = PF_UNSPEC;
            hints.ai_flags = AI_NUMERICHOST;

            int error_code;

            if ((error_code = getaddrinfo(ip_address_string.c_str(), nullptr, &hints, &result)) != 0) {
                DARWIN_LOG_ERROR("GetIpAddressType:: Invalid address: '" + ip_address_string +
                          "' (" + std::strerror(error_code) + ")");

                return false;
            }

            *ip_type = result->ai_family;

            freeaddrinfo(result);

            if (*ip_type != AF_INET && *ip_type != AF_INET6) {
                DARWIN_LOG_ERROR("GetIpAddressType:: Unknown IP address format: " + ip_address_string);

                return false;
            }

            return true;
        }

        void GetSockAddrIn(const std::string &ip_address_string, sockaddr_in *sa) {
            DARWIN_LOGGER;

            inet_pton(AF_INET, ip_address_string.c_str(), &(sa->sin_addr));

            DARWIN_LOG_DEBUG("GetSockAddrIn:: Successful IPv4 parse: " + ip_address_string);
        }

        void GetSockAddrIn6(const std::string &ip_address_string, sockaddr_in6 *sa) {
            DARWIN_LOGGER;

            inet_pton(AF_INET6, ip_address_string.c_str(), &(sa->sin6_addr));

            DARWIN_LOG_DEBUG("GetSockAddrIn6:: Successful IPv6 parse: " + ip_address_string);
        }

        std::string GetStringAddrFromSockAddrIn(const in_addr &addr) {
            char str[INET_ADDRSTRLEN];

            inet_ntop(AF_INET, &addr, str, INET_ADDRSTRLEN);

            return str;
        }

        std::string GetStringAddrFromSockAddrIn6(const in6_addr &addr) {
            char str[INET6_ADDRSTRLEN];

            inet_ntop(AF_INET6, &addr, str, INET6_ADDRSTRLEN);

            return str;
        }

        bool ParsePort(const char* path_address, int& out_port) {
            DARWIN_LOGGER;

            long port = 0;
            if(!darwin::strings::StrToLong(path_address, port)){
                DARWIN_LOG_ERROR("Network::ParsePort:: Error while parsing the port number, unrecognized number: '" + std::string(path_address) + "'");
                return false;
            }

            if (port < 0 || port > 65353) {
                DARWIN_LOG_ERROR("Network::ParsePort:: Error while parsing the port number : out of bounds [0; 65353]: '" + std::string(path_address) + "'");
                return false;
            }

            out_port = static_cast<int>(port);

            return true;
        }

        bool ParseSocketAddress(const std::string& path_address, bool is_udp, 
            NetworkSocketType& out_net_type, boost::asio::ip::address& out_net_address, int& out_port, std::string& out_unix_path) 
        {
            DARWIN_LOGGER;
            size_t colon = path_address.rfind(':');
            if (colon != std::string::npos) {
                boost::system::error_code e;
                std::string addr = path_address.substr(0, colon);
                if(addr.find('[') != std::string::npos && addr.rfind(']') != std::string::npos) {
                    addr.pop_back();
                    addr.erase(0, 1);
                }
                std::string port = path_address.substr(colon+1, path_address.length()-1);
                bool portRes = ParsePort(port.c_str(), out_port);

                if( ! portRes || e.failed()) {
                    DARWIN_LOG_CRITICAL("Network::ParseSocketAddress::Error while parsing the ip address: " + path_address);
                    return false;
                }
                out_net_address = boost::asio::ip::make_address(addr, e); 
                out_net_type= is_udp ? NetworkSocketType::Udp : NetworkSocketType::Tcp;
            } else {
                out_net_type = NetworkSocketType::Unix;
                out_unix_path = path_address;
            }
            return true;
        }

    }
}
