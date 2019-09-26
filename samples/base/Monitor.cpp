/// \file     Monitor.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     12/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <sstream>
#include "Monitor.hpp"
#include "Logger.hpp"

namespace darwin {
    Monitor::Monitor(std::string const& unix_socket_path)
            : _socket_path{unix_socket_path}, _io_context{1},
              _signals{_io_context},
              _acceptor{_io_context,
                        boost::asio::local::stream_protocol::endpoint(
                                _socket_path)}, _connection{_io_context} {
        // Setting the stopping signals for the service
        _signals.add(SIGINT);
        _signals.add(SIGTERM);
#ifdef SIGQUIT
        _signals.add(SIGQUIT);
#endif // !SIGQUIT

        // Start the waiting for a stopping signal
        AwaitStop();

        // Start accepting connections
        Accept();
    }

    void Monitor::AwaitStop() {
        _signals.async_wait(
                boost::bind(&Monitor::HandleStop, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::signal_number));
    }

    void Monitor::HandleStop(
            boost::system::error_code const& error __attribute__((unused)),
            int sig __attribute__((unused))) {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("Server::Handle:: Closing acceptor");
        _acceptor.close();
        _connection.close();
        unlink(_socket_path.c_str());
    }

    void Monitor::Accept() {
        _acceptor.async_accept(_connection,
                               boost::bind(&Monitor::HandleAccept, this,
                                           boost::asio::placeholders::error));
    }

    void Monitor::HandleAccept(boost::system::error_code const& e) {
        DARWIN_LOGGER;

        if (!_acceptor.is_open()) {
            DARWIN_LOG_INFO("Monitor::HandleAccept:: Acceptor closed, "
                     "closing monitor...");
            return;
        }
        if (!e) {
            SendMonitoringData();
        } else {
            DARWIN_LOG_ERROR("Monitor::HandleAccept:: Error accepting connection, "
                             "closing monitor... " + e.message());
        }
    }

    void Monitor::HandleSend(boost::system::error_code const& e,
                             std::size_t size __attribute__((unused))) {
        DARWIN_LOGGER;

        _connection.close();
        if (!e) {
            Accept();
        } else {
            DARWIN_LOG_ERROR(
                    "Monitor::HandleSend:: Error sending data. Stopping monitor... " +
                    e.message());
        }
    }

    void Monitor::SendMonitoringData() {
        boost::asio::async_write(_connection, boost::asio::buffer("{}"),
                                 boost::bind(&Monitor::HandleSend, this,
                                             boost::asio::placeholders::error,
                                             boost::asio::placeholders::bytes_transferred));
    }

    void Monitor::Run() {
        _io_context.run();
    }
}