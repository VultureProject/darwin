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
#include "Session.hpp"
#include "Manager.hpp"
#include "Generator.hpp"

namespace darwin {

    class Server {
    public:
        /// Create an async UNIX stream socket server.
        /// The server runs on nb_threads thread.
        ///
        /// \param socket_path Path of the UNIX socket to listen on.
        /// \param output Filters' output type
        /// \param next_filter_socket Path of the UNIX socket of the filter to send data to.
        /// \param nb_threads Number of thread in the server.
        /// \param threshold Threshold at which the filter will raise a log.
        Server(std::string const& socket_path,
               std::string const& output,
               std::string const& next_filter_socket,
               std::size_t nb_threads,
               std::size_t threshold,
               Generator& generator);

        ~Server() = default;

        // Make the server non copyable & non movable
        Server(Server const&) = delete;

        Server(Server const&&) = delete;

        Server& operator=(Server const&) = delete;

        Server& operator=(Server const&&) = delete;

    public:
        /// Start the server and the threads.
        void Run();

    private:
        /// Start async waiting for the stopping signals.
        void AwaitStop();

        /// Handler called when signal is received by the async wait.
        ///
        /// \param sig The received signal.
        void HandleStop(boost::system::error_code const& error, int sig);

        /// Start an async connection acceptation on _new_session's socket.
        void Accept();

        /// Handler called on async accept trigger.
        void HandleAccept(boost::system::error_code const& e);

    private:
        std::string _socket_path; //!< Path to the UNIX socket to listen on.
        std::string _socket_next; //!< Path to the next filter's UNIX socket.
        std::string _output; //!< Filter's output type
        std::size_t _threshold; //!< Filter's threshold
        boost::asio::io_context _io_context; //!< The async io context.
        boost::asio::signal_set _signals; //!< Set of the stopping signals.
        boost::asio::local::stream_protocol::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::local::stream_protocol::socket _new_connection; //!< Socket used to accept a new connection.
        Generator& _generator; //!< Generator used to create new Sessions.
        Manager _manager; //!< Server's session manager.
    };
}