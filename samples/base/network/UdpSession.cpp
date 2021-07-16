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
        CloseFilterConnection();
    }

    void UdpSession::CloseFilterConnection() {
    }

    void UdpSession::ReadHeader() {
        //NOP
        return;
    }

    void UdpSession::ReadBody(std::size_t size) {
        DARWIN_LOGGER;

        if(size < sizeof(_header)){
            DARWIN_LOG_ERROR("Error parsing header, buffer is less than expected : " + std::to_string(size));
            return;
        }

        std::memcpy(&_header, _buffer.data(), sizeof(_header));

        if(size != sizeof(_header) + _header.body_size){
            DARWIN_LOG_ERROR("Error parsing header sizes, expected " + std::to_string(sizeof(_header) + _header.body_size) + " but buffer is " + std::to_string(size));
            return;
        }

        //TODO Certitudes parsing

        _packet = DarwinPacket(_header);
        auto& body = _packet.GetMutableBody();
        body.append(_buffer.begin() + sizeof(_header), _header.body_size);

        this->ExecuteFilter();
    }

    void UdpSession::WriteToClient(std::vector<unsigned char>& packet) {
        return;
    }

    void UdpSession::ExecuteFilter() {
        ASession::ExecuteFilter();
        // ASession::Start();
    }

    void UdpSession::SetNextFilterPort(int port){
    }
}

