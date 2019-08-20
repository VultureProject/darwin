/// \file     Monitor.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     12/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <boost/asio.hpp>
#include <boost/bind.hpp>

/// \namespace darwin
namespace darwin {

    /// This class is made to handle one connection at the time.
    /// Each connection will receive monitoring data.
    ///
    ///\class Monitor
    class Monitor {

    public:
        /// Constructor.
        ///
        /// \param unix_socket_path The path to the unix socket to connect to the management API.
        explicit Monitor(std::string const& unix_socket_path);
        ~Monitor() = default;

    public:
        /// Main loop of the monitoring thread.
        /// Listen to incoming connection on the monitoring socket to
        /// send back monitoring data.
        ///
        /// \param tp The ThreadPool to monitor.
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

        /// Send the monitoring data through the socket asynchronously.
        void SendMonitoringData();

        /// Called when data is sent using SendMonitoringData() method.
        /// Terminate the session on failure.
        ///
        /// \param size The number of byte sent.
        void HandleSend(boost::system::error_code const& e, std::size_t size);

    private:
        std::string _socket_path; //!< Path to the UNIX socket to listen on.
        boost::asio::io_context _io_context; //!< The async io context.
        boost::asio::signal_set _signals; //!< Set of the stopping signals.
        boost::asio::local::stream_protocol::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::local::stream_protocol::socket _connection; //!< Socket of the current connection.
    };
}