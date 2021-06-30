#pragma once

#include <list>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "ANextFilterConnector.hpp"
#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class UnixNextFilterConnector: public ANextFilterConnector {
    public:
        UnixNextFilterConnector(std::string const& path);
        virtual ~UnixNextFilterConnector() = default;

        virtual bool Connect();

    private:

        virtual void Send(boost::asio::const_buffer const& packet);

        std::string _socket_path;
        boost::asio::local::stream_protocol::socket _socket;
    };
}