#include "UnixNextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    UnixNextFilterConnector::UnixNextFilterConnector(std::string const& path)
     : ANextFilterConnector{}, _socket_path{path}, _socket{_io_context}
    {
    }


    bool UnixNextFilterConnector::Connect() {
        DARWIN_LOGGER;
        boost::system::error_code ec;

        _socket.connect(boost::asio::local::stream_protocol::endpoint(_socket_path), ec);

        if(ec) {
            DARWIN_LOG_ERROR("NextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        return true;
    }

    void UnixNextFilterConnector::Send(boost::asio::const_buffer const& packet) {
        DARWIN_LOGGER;
        if(_nb_attempts > _max_attempts){
            DARWIN_LOG_ERROR("NextFilterConnector::Send:: Maximal number of attempts reached");
            return;
        }
        while( ! this->Connect() && _nb_attempts++ <= _max_attempts) {
            std::this_thread::sleep_for(_attempts_delay_ms);
        }

        boost::asio::async_write(_socket, packet, boost::bind(&UnixNextFilterConnector::SendCallback, this, 
                                                            boost::asio::placeholders::error, 
                                                            boost::asio::placeholders::bytes_transferred,
                                                            packet));

    }
}