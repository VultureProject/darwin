/// \file     Server.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     02/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class UnixServer : public AServer {
    public:
        /// Create an async UNIX stream socket server.
        /// The server runs on nb_threads thread.
        ///
        /// \param socket_path Path of the UNIX socket to listen on.
        /// \param output Filters' output type
        /// \param next_filter_socket Path of the UNIX socket of the filter to send data to.
        /// \param threshold Threshold at which the filter will raise a log.
        UnixServer(std::string const& socket_path,
               std::string const& output,
               std::string const& next_filter_socket,
               std::size_t threshold,
               Generator& generator);

        ~UnixServer() = default;

        // Make the UnixServer non copyable & non movable
        UnixServer(UnixServer const&) = delete;

        UnixServer(UnixServer const&&) = delete;

        UnixServer& operator=(UnixServer const&) = delete;

        UnixServer& operator=(UnixServer const&&) = delete;

        void Clean() override;

        void HandleStop(boost::system::error_code const& error, int sig) override;

        void Accept() override;

        void HandleAccept(boost::system::error_code const& e) override;

    private:
        std::string _socket_path; //!< Path to the UNIX socket to listen on.
        std::string _socket_next; //!< Path to the next filter's UNIX socket.
        boost::asio::local::stream_protocol::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::local::stream_protocol::socket _new_connection; //!< Socket used to accept a new connection.
    };
}