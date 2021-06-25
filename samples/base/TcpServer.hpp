/// \file     Server.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     02/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class TcpServer : public AServer {
    public:
        /// Create an async Tcp stream socket server.
        /// The server runs on nb_threads thread.
        ///
        /// \param socket_path Path of the Tcp socket to listen on.
        /// \param output Filters' output type
        /// \param next_filter_socket Path of the Tcp socket of the filter to send data to.
        /// \param threshold Threshold at which the filter will raise a log.
        TcpServer(boost::asio::ip::address const& address, 
                int port,
                int next_filter_port,
                std::string const& output,
                std::size_t threshold,
                Generator& generator);

        ~TcpServer() = default;

        // Make the TcpServer non copyable & non movable
        TcpServer(TcpServer const&) = delete;

        TcpServer(TcpServer const&&) = delete;

        TcpServer& operator=(TcpServer const&) = delete;

        TcpServer& operator=(TcpServer const&&) = delete;

        void Clean();

        void HandleStop(boost::system::error_code const& error, int sig);

        void Accept();

        void HandleAccept(boost::system::error_code const& e);

    private:
        boost::asio::ip::address _address;
        int _port_nb; //!< Path to the Tcp socket to listen on.
        int _port_nb_next; //!< Path to the next filter's Tcp socket.
        boost::asio::ip::tcp::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::ip::tcp::socket _new_connection; //!< Socket used to accept a new connection.
    };
}