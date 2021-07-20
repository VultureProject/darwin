#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>

#include "Logger.hpp"
#include "TcpSession.hpp"

namespace darwin {
    TcpSession::TcpSession(
        boost::asio::ip::tcp::socket& socket,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _socket{std::move(socket)}
    {
        ;
    }

    void TcpSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("TcpSession::Stop::");
        _socket.close();
    }

    void TcpSession::ReadHeader() {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("ASession::ReadHeader:: Starting to read incoming header...");
        boost::asio::async_read(_socket,
                                boost::asio::buffer(&_header, sizeof(_header)),
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
}

