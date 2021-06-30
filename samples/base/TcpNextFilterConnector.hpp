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
        TcpNextFilterConnector(boost::asio::ip::address const& net_address, int port);
        virtual ~TcpNextFilterConnector() = default;

        virtual bool Connect();

    private:

        virtual void Send(boost::asio::const_buffer const& packet);

        boost::asio::ip::address _net_address;
        int _net_port;
        boost::asio::ip::tcp::socket _socket;
    };
}