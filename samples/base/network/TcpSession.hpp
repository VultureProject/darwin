///
/// \file TcpSession.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Session for TCP Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include "ASession.hpp"
#include "Generator.hpp"

namespace darwin {

    class TcpSession: public ASession {
    public:
        ///
        /// \brief Construct a new Tcp Session object
        /// 
        /// \param socket ref to the socket of a connection, the socket will be moved
        /// \param manager ref to the sessions' manager
        /// \param generator ref to the task generator
        ///
        TcpSession(boost::asio::ip::tcp::socket& socket,
            Manager& manager,
            Generator& generator);

        ~TcpSession() = default;

        void Stop() override final;
        
    protected:

        /// Set the async read for the header.
        ///
        virtual void ReadHeader() override final;

        /// Set the async read for the body.
        ///
        virtual void ReadBody(std::size_t size) override final;

        /// 
        virtual void WriteToClient(std::vector<unsigned char>& packet) override final;

    private:
        boost::asio::ip::tcp::socket _socket; //!< Session's socket.
    };

}