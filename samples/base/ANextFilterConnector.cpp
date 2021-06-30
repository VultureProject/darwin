#include "ANextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    ANextFilterConnector::ANextFilterConnector()
     : _io_context{}, _max_attempts{3}, _attempts_delay_ms{std::chrono::milliseconds(1000)}
    {
    }

    void ANextFilterConnector::Run() {
        _io_context.run();
    }

    void ANextFilterConnector::Send(DarwinPacket& packet) {
        auto b = _buffer_list.insert(_buffer_list.end(), std::move(boost::asio::buffer(packet.Serialize())));
        this->Send(*b);
    }

    void ANextFilterConnector::SendCallback(const boost::system::error_code& ec, size_t bytes_transferred, boost::asio::const_buffer const& buffer) {
        if(ec) {
            Send(buffer);
            return;
        }

        //TODO  _buffer_list.remove(buffer); (remove buffer)
        _nb_attempts = 0;
    }
}