#include "UdpNextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    UdpNextFilterConnector::UdpNextFilterConnector(boost::asio::ip::address const& net_address, int port)
     : ANextFilterConnector{}, _net_address{net_address}, _net_port{port}, _endpoint{_net_address, static_cast<unsigned short>(_net_port)},
        _socket{_io_context}
    {
        
    }


    bool UdpNextFilterConnector::Connect() {
        DARWIN_LOGGER;
        boost::system::error_code ec;

        _socket.connect(_endpoint, ec);

        if(ec) {
            DARWIN_LOG_ERROR("NextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        return true;
    }

    void UdpNextFilterConnector::Send(std::shared_ptr<boost::asio::const_buffer> packet) {
        DARWIN_LOGGER;
        if(_nb_attempts > _max_attempts){
            DARWIN_LOG_ERROR("NextFilterConnector::Send:: Maximal number of attempts reached");
            return;
        }
        while( ! this->Connect() && _nb_attempts++ <= _max_attempts) {
            std::this_thread::sleep_for(_attempts_delay_ms);
        }

        _socket.async_send_to(*packet, 
                            _endpoint, 
                            boost::bind(&UdpNextFilterConnector::SendCallback, this, 
                                boost::asio::placeholders::error, 
                                boost::asio::placeholders::bytes_transferred,
                                packet));
    }
}