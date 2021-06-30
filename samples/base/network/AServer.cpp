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
#include <boost/asio/ip/tcp.hpp>
#include "AServer.hpp"
#include "Logger.hpp"
#include "Stats.hpp"

namespace darwin {

    AServer::AServer(std::string const& output,
                   std::size_t threshold,
                   Generator& generator)
            : _output{output}, _io_context{}, _threshold{threshold}, 
              _signals{_io_context}, _generator{generator} {
    }

    void AServer::InitSignalsAndStart() {
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

    boost::asio::io_context &AServer::GetIOContext() {
        return this->_io_context;
    }

    void AServer::Run() {
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

    void AServer::AwaitStop() {
        _signals.async_wait(
                boost::bind(&AServer::HandleStop, this,
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::signal_number));
    }

}