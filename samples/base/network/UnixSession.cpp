#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "Logger.hpp"
#include "UnixSession.hpp"

namespace darwin {

    UnixSession::UnixSession(
        boost::asio::local::stream_protocol::socket& socket,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _socket{std::move(socket)}
    {
        ;
    }

    void UnixSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("UnixSession::Stop::");
        _socket.close();
    }

    void UnixSession::ReadHeader() {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("ASession::ReadHeader:: Starting to read incoming header...");
        boost::asio::async_read(_socket,
                                boost::asio::buffer(&_header, sizeof(_header)),
                                boost::bind(&UnixSession::ReadHeaderCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UnixSession::ReadBody(std::size_t size) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::ReadBody:: Starting to read incoming body...");

        _socket.async_read_some(boost::asio::buffer(_body_buffer,
                                                    size),
                                boost::bind(&UnixSession::ReadBodyCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UnixSession::WriteToClient(std::vector<unsigned char>& packet) {
        boost::asio::async_write(_socket,
                                boost::asio::buffer(packet),
                                boost::bind(&UnixSession::SendToClientCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }
}

