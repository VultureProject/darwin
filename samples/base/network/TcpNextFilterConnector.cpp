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
            DARWIN_LOG_ERROR("NextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        return true;
    }

    void TcpNextFilterConnector::Send(std::shared_ptr<boost::asio::const_buffer> packet) {
        DARWIN_LOGGER;
        if(_nb_attempts > _max_attempts){
            DARWIN_LOG_ERROR("NextFilterConnector::Send:: Maximal number of attempts reached");
            return;
        }
        while( ! this->Connect() && _nb_attempts++ <= _max_attempts) {
            std::this_thread::sleep_for(_attempts_delay_ms);
        }

        boost::asio::async_write(_socket, *packet, boost::bind(&TcpNextFilterConnector::SendCallback, this, 
                                                            boost::asio::placeholders::error, 
                                                            boost::asio::placeholders::bytes_transferred,
                                                            packet));

    }
}