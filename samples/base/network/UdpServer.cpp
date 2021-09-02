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
#include <boost/asio/ip/udp.hpp>
#include "UdpServer.hpp"
#include "UdpSession.hpp"
#include "Logger.hpp"
#include "Stats.hpp"

namespace darwin {

    UdpServer::UdpServer(boost::asio::ip::address const& address,
                   int port_nb,
                   std::string const& output,
                   std::size_t threshold,
                   Generator& generator)
            : AServer(output, threshold, generator), 
              _address{address}, _port_nb{port_nb}, 
              _endpoint{_address, static_cast<unsigned short>(_port_nb)}, 
              _new_connection{_io_context,_endpoint} 
    {
        this->InitSignalsAndStart();
    }

    
    

    void UdpServer::Clean() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("Server::Clean:: Cleaning server...");
        _manager.StopAll();
    }

    void UdpServer::HandleStop(boost::system::error_code const& error __attribute__((unused)), int sig __attribute__((unused))) {
        // The server is stopped by cancelling all outstanding asynchronous
        // operations. Once all operations have finished the io_context::run()
        // call will exit.
        DARWIN_LOGGER;

        SET_FILTER_STATUS(darwin::stats::FilterStatusEnum::stopping);
        DARWIN_LOG_DEBUG("Server::Handle:: Closing acceptor");
        _io_context.stop();
    }

    void UdpServer::Accept() {
        _new_connection.async_receive(boost::asio::buffer(_buffer),
                                boost::bind(&UdpServer::HandleReceive, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UdpServer::HandleReceive(boost::system::error_code const& e, size_t bytes_received) {
        DARWIN_LOGGER;

        if (!e) {
            if(bytes_received >= sizeof(darwin_filter_packet_t)){
                DARWIN_LOG_DEBUG("UdpServer::HandleReceive:: New connection accepted");
                auto sess = std::make_shared<UdpSession>(_buffer, _manager, _generator);
                sess->SetOutputType(_output);
                sess->SetThreshold(_threshold);
                sess->ReadBody(bytes_received);
            } else {
                DARWIN_LOG_ERROR("UdpServer::HandleReceive:: packet too small (in bytes) : " + std::to_string(bytes_received));
            }
        } else {
            DARWIN_LOG_ERROR("UdpServer::HandleReceive:: Error accepting connection, no longer accepting");
        }
        Accept();
    }

    void UdpServer::HandleAccept(boost::system::error_code const& e __attribute__((unused))) {
        return;
    }

    
}