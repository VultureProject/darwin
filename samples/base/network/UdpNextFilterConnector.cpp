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
            // We handle the case but, since UDP is stateless, this _socket.connect call does nothing and never fails
            DARWIN_LOG_ERROR("UdpNextFilterConnector::Connect:: Connexion error : " + std::string(ec.message()));
            return false;
        }
        return true;
    }

    void UdpNextFilterConnector::Send(std::shared_ptr<boost::asio::const_buffer> packet) {
        DARWIN_LOGGER;
        // The Connect call cannot fail, we don't have to check and we don't have to retry it
        this->Connect();

        DARWIN_LOG_DEBUG("UdpNextFilterConnector::Send : Sending packet to UDP endpoint, no retries");

        _socket.async_send_to(*packet, 
                            _endpoint, 
                            boost::bind(&UdpNextFilterConnector::SendCallback, this, 
                                boost::asio::placeholders::error, 
                                boost::asio::placeholders::bytes_transferred,
                                packet));
    }
}