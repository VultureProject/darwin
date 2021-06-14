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
#include "TcpServer.hpp"
#include "Logger.hpp"
#include "Stats.hpp"

namespace darwin {

    TcpServer::TcpServer(std::string const& socket_path,
                   std::string const& output,
                   std::string const& next_filter_socket,
                   std::size_t threshold,
                   Generator& generator)
            : AServer(output, threshold, generator), 
              _socket_path{socket_path}, _socket_next{next_filter_socket},
              _acceptor{_io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 8000)},
              _new_connection{_io_context} {

        this->InitSignalsAndStart();
    }
    

    void TcpServer::Clean() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Server::Clean:: Cleaning server...");
        _manager.StopAll();
        //todo: can't work, to be changed
        unlink(_socket_path.c_str());
    }

    void TcpServer::HandleStop(boost::system::error_code const& error __attribute__((unused)), int sig __attribute__((unused))) {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        DARWIN_LOGGER;

        SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::stopping);
        DARWIN_LOG_DEBUG("Server::Handle:: Closing acceptor");
        _acceptor.close();
        _io_context.stop();
    }

    void TcpServer::Accept() {
        _acceptor.async_accept(_new_connection,
                boost::bind(&TcpServer::HandleAccept, this,
                            boost::asio::placeholders::error));
    }

    void TcpServer::HandleAccept(boost::system::error_code const& e) {
        DARWIN_LOGGER;

        if (!_acceptor.is_open()) {
            DARWIN_LOG_INFO("Server::HandleAccept:: Acceptor closed, closing server...");
            return;
        }

        if (!e) {
            DARWIN_LOG_DEBUG("Server::HandleAccept:: New connection accepted");
            //todo to be modified

            // auto task = _generator.CreateTask(_new_connection, _manager);
            // task->SetNextFilterSocketPath(_socket_next);
            // task->SetOutputType(_output);
            // task->SetThreshold(_threshold);
            // _manager.Start(task);
            Accept();
        } else {
            DARWIN_LOG_ERROR("Server::HandleAccept:: Error accepting connection, no longer accepting");
        }
    }
    
}