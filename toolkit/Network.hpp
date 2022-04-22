/// \file     DecisionTask.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     17/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once


extern "C" {
#include <netinet/in.h>
}

#include <string>

/// \namespace darwin
namespace darwin {
    /// \namespace network
    namespace network {

        enum NetworkSocketType {
            Unix,
            Tcp,
            Udp
        };

        /// Get the IP address type.
        ///
        /// \param ip_address_string The IP address to get the type from.
        /// \param ip_type The integer to set the result with.
        /// \return true on success, false otherwise.
        bool GetIpAddressType(const std::string &ip_address_string, int *ip_type);

        /// Get an IPv4 sockaddr structure from an IPv4 IP address string.
        ///
        /// \param ip_address_string The IP address to get the sockaddrr structure from.
        /// \param sa The sockaddr structure to be saved.
        void GetSockAddrIn(const std::string &ip_address_string, sockaddr_in *sa);

        /// Get an IPv6 sockaddr structure from an IPv4 IP address string.
        ///
        /// \param ip_address_string The IP address to get the sockaddrr structure from.
        /// \param sa The sockaddr structure to be saved.
        void GetSockAddrIn6(const std::string &ip_address_string, sockaddr_in6 *sa);

        /// Get an IPv4 address string from an IPv4 in_addr structure.
        ///
        /// \param sa The in_addr structure to be stringified.
        std::string GetStringAddrFromSockAddrIn(const in_addr &addr);

        /// Get an IPv6 address string from an IPv6 in_addr structure.
        ///
        /// \param sa The in6_addr structure to be stringified.
        std::string GetStringAddrFromSockAddrIn6(const in6_addr &addr);

        ///
        /// \brief Parse a string to get the socket path if any or the ipv4/v6 address
        /// 
        /// \param path_address the string to parse
        /// \param is_udp true if we should use udp instead of tcp
        /// \param out_net_type type of address/path parsed
        /// \param out_net_address ip address to fill when parsed
        /// \param out_port ip port to fill when parsed
        /// \param out_unix_path unix sock path to fill when parsed
        /// \return true if the parsing was successful
        ///
        bool ParseSocketAddress(const std::string& path_address, bool is_udp, 
            NetworkSocketType& out_net_type, boost::asio::ip::address& out_net_address, int& out_port, std::string& out_unix_path);

        ///
        /// \brief parse an ip port, returns early if there is an error while parsing or if the port is not a valid port number
        /// 
        /// \param str_port port to parse
        /// \param out_port port to fill with the port
        /// \return true if the parsing was successful
        ///
        bool ParsePort(const std::string& str_port, int& out_port);
    }
}
