///
/// \file UnixNextFilterConnector.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Connector to the NExt filter for Unix socket Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///

#include <list>
#include <chrono>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "ANextFilterConnector.hpp"
#include "DarwinPacket.hpp"
#include "Network.hpp"

namespace darwin {

    class UnixNextFilterConnector: public ANextFilterConnector {
    public:
        ///
        /// \brief Construct a new Unix Next Filter Connector object
        /// 
        /// \param path path of the unix socket of the next filter
        ///
        UnixNextFilterConnector(std::string const& path);
        virtual ~UnixNextFilterConnector() = default;

        virtual bool Connect();

    private:

        virtual void Send(std::shared_ptr<boost::asio::const_buffer> packet);

        std::string _socket_path;
        boost::asio::local::stream_protocol::socket _socket;
    };
}