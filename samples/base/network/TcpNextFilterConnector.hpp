///
/// \file TcpNextFilterConnector.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Connection to NExt Filter (TCP Protocol)
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
#include <boost/asio/ip/tcp.hpp>

#include "ANextFilterConnector.hpp"
#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class TcpNextFilterConnector: public ANextFilterConnector {
    public:
        ///
        /// \brief Construct a new Tcp Next Filter Connector object
        /// 
        /// \param net_address parsed ip address of the next filter
        /// \param port port of the next filter
        ///
        TcpNextFilterConnector(boost::asio::ip::address const& net_address, int port);
        virtual ~TcpNextFilterConnector() = default;

        ///
        /// \brief Attempts to connect to the next Filter
        /// 
        /// \return true if the attempt is successful
        /// \return false else
        ///
        virtual bool Connect();

    private:

        virtual void Send(std::shared_ptr<boost::asio::const_buffer> packet);

        boost::asio::ip::address _net_address;
        int _net_port;
        boost::asio::ip::tcp::socket _socket;
    };
}