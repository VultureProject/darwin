#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>

#include "Logger.hpp"
#include "UdpSession.hpp"

namespace darwin {
    UdpSession::UdpSession(const udp_buffer_t& buffer,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _buffer{buffer}
    {
        ;
    }

    void UdpSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("UdpSession::Stop::");
    }

    void UdpSession::ReadHeader() {
        //NOP
        return;
    }

    void UdpSession::ReadBody(std::size_t size) {
        DARWIN_LOGGER;

        if(size < sizeof(_header)){
            DARWIN_LOG_ERROR("UdpSession::ReadBody: Error parsing header, buffer is less than expected : " + std::to_string(size));
            return;
        }

        std::memcpy(&_header, _buffer.data(), sizeof(_header));
        
        //The header can hold one certitude, if there are more, we have to parsed them accordingly
        size_t cert_size = _header.certitude_size * sizeof(unsigned int);
        if(size != sizeof(_header) + _header.body_size + cert_size){
            DARWIN_LOG_ERROR("UdpSession::ReadBody: Error parsing header sizes, expected " + std::to_string(sizeof(_header) + _header.body_size + cert_size) + " but buffer is " + std::to_string(size));
            return;
        }

        _packet = DarwinPacket(_header);

        for(size_t i=0; i < _header.certitude_size; i++){
            unsigned int cert = 0;
            std::memcpy(&cert, _buffer.data() + sizeof(_header) + i*sizeof(unsigned int), sizeof(unsigned int));
            _packet.AddCertitude(cert);
        }

        std::string& body = _packet.GetMutableBody();
        body.append(_buffer.begin() + sizeof(_header) + cert_size, _header.body_size);

        this->ExecuteFilter();
    }

    void UdpSession::WriteToClient(std::vector<unsigned char>& packet __attribute((unused))) {
        return;
    }
}

