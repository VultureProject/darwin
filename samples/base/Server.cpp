/// \file     Server.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     02/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <iostream>
#include <functional>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "Server.hpp"
#include "Logger.hpp"

namespace darwin {

    Server::Server(std::string const& socket_path,
                   std::string const& output,
                   std::string const& next_filter_socket,
                   std::size_t threshold,
                   Generator& generator)
            : _socket_path{socket_path}, _socket_next{next_filter_socket}, _output{output},
              _io_context{}, _threshold{threshold}, _signals{_io_context},
              _acceptor{_io_context,
                        boost::asio::local::stream_protocol::endpoint(
                                socket_path)},
              _new_connection{_io_context}, _generator{generator} {

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

    void Server::Run() {
        // The io_context::run() call will block until all asynchronous operations
        // have finished. While the server is running, there is always at least one
        // asynchronous operation outstanding: the asynchronous accept call waiting
        // for new incoming connections.

        // By using multiple threads, a program can call run() multiple times.
        // Once an asynchronous operation is complete,
        // the I/O service object will execute the handler in one of these threads.
        // If a second operation is completed shortly after the first one,
        // the I/O service object can execute the handler in a different thread.
        // Now, not only can operations outside of a process be executed concurrently,
        // but handlers within the process can be executed concurrently, too.
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Server::Run:: Running...");
        _io_context.run();
    }

    void Server::Clean() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Server::Clean:: Cleaning server...");
        _manager.StopAll();
        unlink(_socket_path.c_str());
    }

    void Server::AwaitStop() {
        _signals.async_wait(
                boost::bind(&Server::HandleStop, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::signal_number));
    }

    void Server::HandleStop(boost::system::error_code const& error __attribute__((unused)), int sig __attribute__((unused))) {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("Server::Handle:: Closing acceptor");
        _acceptor.close();
        _io_context.stop();
    }

    void Server::Accept() {
        _acceptor.async_accept(_new_connection,
                boost::bind(&Server::HandleAccept, this,
                            boost::asio::placeholders::error));
    }

    void Server::HandleAccept(boost::system::error_code const& e) {
        DARWIN_LOGGER;

        if (!_acceptor.is_open()) {
            DARWIN_LOG_INFO("Server::HandleAccept:: Acceptor closed, closing server...");
            return;
        }

        if (!e) {
            DARWIN_LOG_DEBUG("Server::HandleAccept:: New connection accepted");
            auto task = _generator.CreateTask(_new_connection, _manager);
            task->SetNextFilterSocketPath(_socket_next);
            task->SetOutputType(_output);
            task->SetThreshold(_threshold);
            _manager.Start(task);
            Accept();
        } else {
            DARWIN_LOG_ERROR("Server::HandleAccept:: Error accepting connection, no longer accepting");
        }
    }

}