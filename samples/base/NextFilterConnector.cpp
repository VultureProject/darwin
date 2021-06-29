#include "NextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    NextFilterConnector::NextFilterConnector(std::string const& path_address, bool is_udp)
     : _io_context{}, _tcp_socket{_io_context}, _unix_socket{_io_context}, 
     _max_attempts{3}, _attempts_delay_ms{std::chrono::milliseconds(1000)}
    {
        _address_path_parsing = network::ParseSocketAddress(path_address, is_udp, _net_type, _net_address, _net_port, _socketPath);
    }

    bool NextFilterConnector::Run() {
        if (! _address_path_parsing) {
            return false;
        }
        _io_context.run();
        return true;
    }

    bool NextFilterConnector::Connect() {
        DARWIN_LOGGER;
        boost::system::error_code ec;
        switch(_net_type) {
            case network::NetworkSocketType::Unix:
                _unix_socket.connect(boost::asio::local::stream_protocol::endpoint(_socketPath), ec);
                break;
            case network::NetworkSocketType::Tcp:
                _tcp_socket.connect(boost::asio::ip::tcp::endpoint(_net_address, _net_port), ec);
                break;
            case network::NetworkSocketType::Udp:
                DARWIN_LOG_CRITICAL("NextFilterConnector::Connect:: UDP Not implemented");
                return false;
            default:
                DARWIN_LOG_CRITICAL("NextFilterConnector::Connect:: Unhandled case");
                return false;
        }

        if(ec) {
            DARWIN_LOG_ERROR("NextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        return true;
    }

    void NextFilterConnector::Send(DarwinPacket& packet) {
        auto b = _buffer_list.insert(_buffer_list.end(), std::move(boost::asio::buffer(packet.Serialize())));
        this->Send(*b);
    }

    void NextFilterConnector::Send(boost::asio::const_buffer const& packet) {
        DARWIN_LOGGER;
        if(_nb_attempts > _max_attempts){
            DARWIN_LOG_ERROR("NextFilterConnector::Send:: Maximal number of attempts reached");
            return;
        }
        while( ! this->Connect() && _nb_attempts++ <= _max_attempts) {
            std::this_thread::sleep_for(_attempts_delay_ms);
        }

        switch(_net_type) {
            case network::NetworkSocketType::Unix:
                boost::asio::async_write(_unix_socket, packet, boost::bind(&NextFilterConnector::SendCallback, this, 
                                                                    boost::asio::placeholders::error, 
                                                                    boost::asio::placeholders::bytes_transferred,
                                                                    packet));
                break;
            case network::NetworkSocketType::Tcp:
                boost::asio::async_write(_tcp_socket, packet, boost::bind(&NextFilterConnector::SendCallback, this, 
                                                                    boost::asio::placeholders::error, 
                                                                    boost::asio::placeholders::bytes_transferred,
                                                                    packet));
                break;
            case network::NetworkSocketType::Udp:

                break;
        }
    }

    void NextFilterConnector::SendCallback(const boost::system::error_code& ec, size_t bytes_transferred, boost::asio::const_buffer const& buffer) {
        if(ec) {
            Send(buffer);
            return;
        }

        //TODO  _buffer_list.remove(buffer); (remove buffer)
        _nb_attempts = 0;
    }
}