///
/// \file UdpServer.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Server for UDP Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/udp.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class UdpServer : public AServer {
    public:
        ///
        /// \brief Construct a new Udp Server object
        /// 
        /// \param address parsed ip address of this filter
        /// \param port port of this filter
        /// \param output filter's output type
        /// \param threshold filter's threshold
        /// \param generator ref to the task generator
        ///
        UdpServer(boost::asio::ip::address const& address, 
                int port,
                std::string const& output,
                std::size_t threshold,
                Generator& generator);

        virtual ~UdpServer() = default;

        // Make the UdpServer non copyable & non movable
        UdpServer(UdpServer const&) = delete;

        UdpServer(UdpServer const&&) = delete;

        UdpServer& operator=(UdpServer const&) = delete;

        UdpServer& operator=(UdpServer const&&) = delete;

        void Clean();

        void HandleStop(boost::system::error_code const& error, int sig);

        void Accept();

        void HandleAccept(boost::system::error_code const& e);

        void HandleReceive(boost::system::error_code const& e, size_t bytes_received);
    private:
        boost::asio::ip::address _address;
        int _port_nb; //!< Path to the Udp socket to listen on.
        boost::asio::ip::udp::endpoint _endpoint;
        boost::asio::ip::udp::socket _new_connection; //!< Socket used to accept a new connection.
        udp_buffer_t _buffer;
    };
}