#include "TcpNextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    TcpNextFilterConnector::TcpNextFilterConnector(boost::asio::ip::address const& net_address, int port)
     : ANextFilterConnector{}, _net_address{net_address}, _net_port{port}, _socket{_io_context}
    {
        
    }


    bool TcpNextFilterConnector::Connect() {
        DARWIN_LOGGER;
        boost::system::error_code ec;

        _socket.connect(boost::asio::ip::tcp::endpoint(_net_address, _net_port), ec);

        if(ec) {
            _nb_attempts++;
            DARWIN_LOG_ERROR("TcpNextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        // Reset the counter on success
        _nb_attempts = 0;
        _is_connected = true;
        return true;
    }

    void TcpNextFilterConnector::Send(std::shared_ptr<boost::asio::const_buffer> packet) {
        DARWIN_LOGGER;
        if(! _is_connected){
            while( ! this->Connect()) {
                if(_nb_attempts >= _max_attempts){
                    DARWIN_LOG_ERROR("TcpNextFilterConnector::Send:: Maximal number of attempts reached, aborting packet");
                    return;
                }
                std::this_thread::sleep_for(_attempts_delay_ms);
            }
        }

        boost::asio::async_write(_socket, *packet, boost::bind(&TcpNextFilterConnector::SendCallback, this, 
                                                            boost::asio::placeholders::error, 
                                                            boost::asio::placeholders::bytes_transferred,
                                                            packet));

    }
}