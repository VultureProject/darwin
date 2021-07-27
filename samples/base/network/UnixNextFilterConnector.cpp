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
            _nb_attempts++;
            DARWIN_LOG_ERROR("UnixNextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        // Reset the counter on success
        _nb_attempts = 0;
        _is_connected = true;
        return true;
    }

    void UnixNextFilterConnector::Send(std::shared_ptr<boost::asio::const_buffer> packet) {
        DARWIN_LOGGER;
        if(! _is_connected){
            while( ! this->Connect()) {
                if(_nb_attempts >= _max_attempts){
                    DARWIN_LOG_ERROR("UnixNextFilterConnector::Send:: Maximal number of attempts reached, aborting packet");
                    return;
                }
                std::this_thread::sleep_for(_attempts_delay_ms);
            }
        }
        boost::asio::async_write(_socket, *packet, boost::bind(&UnixNextFilterConnector::SendCallback, this, 
                                                            boost::asio::placeholders::error, 
                                                            boost::asio::placeholders::bytes_transferred,
                                                            packet));

    }
}