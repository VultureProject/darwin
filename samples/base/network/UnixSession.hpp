///
/// \file UnixSession.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Session for Unix Socket Protocol
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include "ASession.hpp"

namespace darwin {

    class UnixSession: public ASession {
    public:
        ///
        /// \brief Construct a new Unix Session object
        /// 
        /// \param socket ref to the unix socket, it will be moved to the object
        /// \param manager ref to the sessions' manager
        /// \param generator ref to the task generator
        ///
        UnixSession(boost::asio::local::stream_protocol::socket& socket,
            Manager& manager,
            Generator& generator);

        ~UnixSession() = default;

        void Stop() override final;

        /// Set the path to the associated decision module UNIX socket
        ///
        /// \param path Path to the UNIX socket.
        void SetNextFilterSocketPath(std::string const& path);


    protected:

        /// Set the async read for the header.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadHeader() override final;

        /// Set the async read for the body.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadBody(std::size_t size) override final;

        /// 
        virtual void WriteToClient(std::vector<unsigned char>& packet) override final;

    private:
        boost::asio::local::stream_protocol::socket _socket; //!< Session's socket.
    };

}