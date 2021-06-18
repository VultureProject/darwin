#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>

#include "Logger.hpp"
#include "TcpSession.hpp"

namespace darwin {
    TcpSession::TcpSession(
        boost::asio::ip::tcp::socket& socket,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _connected{false}, 
            _socket{std::move(socket)},
            _filter_socket{socket.get_executor()}, _next_filter_port{0}
    {
        ;
    }

    void TcpSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("TcpSession::Stop::");
        _socket.close();
        CloseFilterConnection();
    }

    void TcpSession::CloseFilterConnection() {
        if(_connected){
            _filter_socket.close();
            _connected = false;
        }
    }

    void TcpSession::ReadHeader() {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("ASession::ReadHeader:: Starting to read incoming header...");
        boost::asio::async_read(_socket,
                                boost::asio::buffer(_header_buffer, DarwinPacket::getMinimalSize()),
                                boost::bind(&TcpSession::ReadHeaderCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void TcpSession::ReadBody(std::size_t size) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::ReadBody:: Starting to read incoming body...");

        _socket.async_read_some(boost::asio::buffer(_body_buffer,
                                                    size),
                                boost::bind(&TcpSession::ReadBodyCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void TcpSession::WriteToClient(std::vector<unsigned char>& packet) {
        boost::asio::async_write(_socket,
                                boost::asio::buffer(packet),
                                boost::bind(&TcpSession::SendToClientCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void TcpSession::WriteToFilter(darwin_filter_packet_t* packet, size_t packet_size) {
        boost::asio::async_write(_filter_socket,
                            boost::asio::buffer(packet, packet_size),
                            boost::bind(&TcpSession::SendToFilterCallback, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));

    }

    void TcpSession::SetNextFilterPort(int port){
        if(port > 0 && port < 65536) {
            _next_filter_port = port;
            _has_next_filter = true;
        }
    }

    bool TcpSession::ConnectToNextFilter() {
        DARWIN_LOGGER;
        if (!_connected) {
            DARWIN_LOG_DEBUG("TcpSession::SendToFilter:: Trying to connect to: " +
                             std::to_string(_next_filter_port));
            try {
                _filter_socket.connect(
                        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), _next_filter_port));
                _connected = true;
            } catch (std::exception const& e) {
                DARWIN_LOG_ERROR(std::string("TcpSession::SendToFilter:: "
                                             "Unable to connect to next filter: ") +
                                 e.what());
                _connected = false;
            }
        }
        return _connected;
    }
}

