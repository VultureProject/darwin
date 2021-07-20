///
/// \file UdpNextFilterConnector.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Connector to Next Filter for UDP Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include <list>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>

#include "ANextFilterConnector.hpp"
#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class UdpNextFilterConnector: public ANextFilterConnector {
    public:
        ///
        /// \brief Construct a new Udp Next Filter Connector object
        /// 
        /// \param net_address parsed IP address of the next filter
        /// \param port port of the next filter
        ///
        UdpNextFilterConnector(boost::asio::ip::address const& net_address, int port);
        virtual ~UdpNextFilterConnector() = default;

        virtual bool Connect();

    private:

        virtual void Send(std::shared_ptr<boost::asio::const_buffer> packet);

        boost::asio::ip::address _net_address;
        int _net_port;
        boost::asio::ip::udp::endpoint _endpoint;
        boost::asio::ip::udp::socket _socket;
    };
}