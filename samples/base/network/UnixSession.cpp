#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "Logger.hpp"
#include "UnixSession.hpp"

namespace darwin {

    UnixSession::UnixSession(
        boost::asio::local::stream_protocol::socket& socket,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _connected{false}, 
            _socket{std::move(socket)},
            _filter_socket{socket.get_executor()} 
    {
        ;
    }

    void UnixSession::SetNextFilterSocketPath(std::string const& path){
        if(path.compare("no") != 0) {
            _next_filter_path = path;
            _has_next_filter = true;
        }
    }

    void UnixSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("UnixSession::Stop::");
        _socket.close();
        if(_connected) {
            _filter_socket.close();
            _connected = false;
        }
    }

    void UnixSession::CloseFilterConnection() {
        if(_connected){
            _filter_socket.close();
            _connected = false;
        }
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

