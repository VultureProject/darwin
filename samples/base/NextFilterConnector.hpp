#pragma once

#include <list>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class NextFilterConnector {
    public:
        NextFilterConnector(std::string const& path_address, bool is_udp);
        ~NextFilterConnector() = default;

        bool Run();

        bool Connect();

        void Send(DarwinPacket& packet);

    private:

        void Send(boost::asio::const_buffer const& packet);

        void SendCallback(const boost::system::error_code& e, size_t size, boost::asio::const_buffer const& buffer);

        network::NetworkSocketType _net_type;
        std::string _socketPath;
        boost::asio::ip::address _net_address;
        int _net_port;

        boost::asio::io_context _io_context;
        boost::asio::ip::tcp::socket _tcp_socket;
        boost::asio::local::stream_protocol::socket _unix_socket;
        std::list<boost::asio::const_buffer> _buffer_list;

        size_t _max_attempts;
        size_t _nb_attempts;
        std::chrono::milliseconds _attempts_delay_ms;
        bool _address_path_parsing;
    };
}