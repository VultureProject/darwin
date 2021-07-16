/// \file     Server.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     02/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class UdpServer : public AServer {
    public:
        /// Create an async Udp stream socket server.
        /// The server runs on nb_threads thread.
        ///
        /// \param socket_path Path of the Udp socket to listen on.
        /// \param output Filters' output type
        /// \param next_filter_socket Path of the Udp socket of the filter to send data to.
        /// \param threshold Threshold at which the filter will raise a log.
        UdpServer(boost::asio::ip::address const& address, 
                int port,
                std::string const& output,
                std::size_t threshold,
                Generator& generator);

        virtual ~UdpServer() = default;

        // Make the UdpServer non copyable & non movable
        UdpServer(UdpServer const&) = delete;

        UdpServer(UdpServer const&&) = delete;

        UdpServer& operator=(UdpServer const&) = delete;

        UdpServer& operator=(UdpServer const&&) = delete;

        void Clean();

        void HandleStop(boost::system::error_code const& error, int sig);

        void Accept();

        void HandleAccept(boost::system::error_code const& e);

        void HandleReceive(boost::system::error_code const& e, size_t bytes_received);
    private:
        boost::asio::ip::address _address;
        int _port_nb; //!< Path to the Udp socket to listen on.
        boost::asio::ip::udp::endpoint _endpoint;
        boost::asio::ip::udp::socket _new_connection; //!< Socket used to accept a new connection.
        udp_buffer_t _buffer;
    };
}