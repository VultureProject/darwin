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