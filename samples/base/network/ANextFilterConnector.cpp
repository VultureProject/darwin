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
        std::vector<unsigned char> v = packet.Serialize();
        std::shared_ptr<boost::asio::const_buffer> buf_ptr = std::make_shared<boost::asio::const_buffer>(v.data(), v.size());
        _buffer_set.insert(buf_ptr);
        this->Send(buf_ptr);
    }

    void ANextFilterConnector::SendCallback(const boost::system::error_code& ec, size_t bytes_transferred, std::shared_ptr<boost::asio::const_buffer> buffer) {
        if(ec || bytes_transferred != buffer->size()) {
            Send(buffer);
            return;
        }

        _buffer_set.erase(buffer);
        _nb_attempts = 0;
    }
}