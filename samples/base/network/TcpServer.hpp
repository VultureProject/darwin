///
/// \file TcpServer.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Server for TCP Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class TcpServer : public AServer {
    public:
        ///
        /// \brief Construct a new Tcp Server object
        /// 
        /// \param address parsed ip address of this filter
        /// \param port port of this filter
        /// \param output filter's output type
        /// \param threshold filter's threshold
        /// \param generator ref to the task generator
        ///
        TcpServer(boost::asio::ip::address const& address, 
                int port,
                std::string const& output,
                std::size_t threshold,
                Generator& generator);

        virtual ~TcpServer() = default;

        // Make the TcpServer non copyable & non movable
        TcpServer(TcpServer const&) = delete;

        TcpServer(TcpServer const&&) = delete;

        TcpServer& operator=(TcpServer const&) = delete;

        TcpServer& operator=(TcpServer const&&) = delete;

        void Clean();

        void HandleStop(boost::system::error_code const& error, int sig);

        void Accept();

        void HandleAccept(boost::system::error_code const& e);

    private:
        boost::asio::ip::address _address;
        int _port_nb; //!< Path to the Tcp socket to listen on.
        boost::asio::ip::tcp::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::ip::tcp::socket _new_connection; //!< Socket used to accept a new connection.
    };
}