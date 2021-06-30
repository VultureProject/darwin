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
#include "TcpSession.hpp"
#include "Logger.hpp"
#include "Stats.hpp"

namespace darwin {

    TcpServer::TcpServer(boost::asio::ip::address const& address,
                   int port_nb,
                   std::string const& output,
                   std::size_t threshold,
                   Generator& generator)
            : AServer(output, threshold, generator), 
              _address{address}, _port_nb{port_nb}, 
              _acceptor{_io_context, boost::asio::ip::tcp::endpoint(_address, port_nb)},
              _new_connection{_io_context} {

        this->InitSignalsAndStart();
    }
    

    void TcpServer::Clean() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Server::Clean:: Cleaning server...");
        _manager.StopAll();
        //todo: can't work, to be changed
        // unlink? what? why?
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
            auto sess = std::make_shared<TcpSession>(_new_connection, _manager, _generator);
            sess->SetOutputType(_output);
            sess->SetThreshold(_threshold);
            _manager.Start(sess);
            Accept();
        } else {
            DARWIN_LOG_ERROR("Server::HandleAccept:: Error accepting connection, no longer accepting");
        }
    }
    
}