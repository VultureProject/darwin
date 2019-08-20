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
#include "Logger.hpp"

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
    }
}
