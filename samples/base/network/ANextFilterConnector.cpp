#include "ANextFilterConnector.hpp"

#include <boost/bind.hpp>

#include "Logger.hpp"

namespace darwin {

    ANextFilterConnector::ANextFilterConnector()
     : _io_context{}, _nb_attempts{0}, _max_attempts{3}, _attempts_delay_ms{std::chrono::milliseconds(1000)}, _is_connected{false}
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
        DARWIN_LOGGER;
        if(ec || bytes_transferred != buffer->size()) {
            if(ec){
                _is_connected = false;
                DARWIN_LOG_NOTICE("ANextFilterConnector::SendCallback Error when sending to Next Filter, retrying, error:" + ec.message());
            } else {
                DARWIN_LOG_NOTICE("ANextFilterConnector::SendCallback Error when sending to Next Filter, retrying, error: mismatched size, was :"
                                     + std::to_string(bytes_transferred) + ", expected:" + std::to_string(buffer->size()));
            }
            Send(buffer);
            return;
        }

        _buffer_set.erase(buffer);
        _nb_attempts = 0;
    }
}