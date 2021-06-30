#pragma once

#include <list>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class ANextFilterConnector {
    public:
        ANextFilterConnector();
        virtual ~ANextFilterConnector() = default;

        void Run();

        virtual bool Connect() = 0;

        void Send(DarwinPacket& packet);

    protected:

        virtual void Send(boost::asio::const_buffer const& packet) = 0;

        virtual /*?*/ void SendCallback(const boost::system::error_code& e, size_t size, boost::asio::const_buffer const& buffer);

        boost::asio::io_context _io_context;
        std::list<boost::asio::const_buffer> _buffer_list;

        size_t _max_attempts;
        size_t _nb_attempts;
        std::chrono::milliseconds _attempts_delay_ms;
        bool _address_path_parsing;
    };
}