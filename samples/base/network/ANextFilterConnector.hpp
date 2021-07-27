///
/// \file ANextFilterConnector.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Abstract Connector to the Next Filter to be extended for different protocols
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include <set>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class ANextFilterConnector {
    public:
        ANextFilterConnector();
        virtual ~ANextFilterConnector() = default;

        ///
        /// \brief Runs the io_context from the class
        /// 
        ///
        virtual void Run() final;

        ///
        /// \brief Tries to connect to the next filter
        /// 
        /// \return true if the connection is established
        /// \return false else
        ///
        virtual bool Connect() = 0;

        ///
        /// \brief Attempt to send a DarwinPacket to the next filter, it will atttempt to connect by calling this->Connect()
        ///        The Packet will be copied to a boost::asio::const_buffer and kept in a set
        ///        There will be _max_attempts tries before aborting the packet
        /// 
        /// \param packet packet to be sent
        ///
        virtual void Send(DarwinPacket& packet);

    protected:

        ///
        /// \brief Attempt to send a packet to the next filter, it will atttempt to connect by calling this->Connect()
        ///        The Packet will be copied to a boost::asio::const_buffer and kept in a set
        ///        There will be _max_attempts tries before aborting the packet
        /// 
        /// \param packet a shared pointer to a boost asio buffer to be sent
        ///
        virtual void Send(std::shared_ptr<boost::asio::const_buffer> packet) = 0;

        ///
        /// \brief Callback called by the io_context when the async 'Send' operation has ended
        ///        This Callback will call 'Send(buffer)' if there is an error
        /// 
        /// \param e error code returned
        /// \param size number of bytes sent
        /// \param buffer shared pointer to buffer sent 
        ///
        virtual void SendCallback(const boost::system::error_code& e, size_t size, std::shared_ptr<boost::asio::const_buffer> buffer);

        boost::asio::io_context _io_context;
        std::set<std::shared_ptr<boost::asio::const_buffer>> _buffer_set;

        size_t _nb_attempts;
        size_t _max_attempts;
        std::chrono::milliseconds _attempts_delay_ms;
        bool _is_connected;
    };
}