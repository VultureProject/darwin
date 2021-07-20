/// \file     UnixServer.hpp
/// \authors  hsoszynski
/// \version  2.0
/// \date     02/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018-2021 Advens. All rights reserved.

#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include "UnixSession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
#include "AServer.hpp"

namespace darwin {

    class UnixServer : public AServer {
    public:
        ///
        /// \brief Construct a new Unix Server object
        /// 
        /// \param socket_path Path of the UNIX socket to listen on
        /// \param output Filters' output type
        /// \param threshold Threshold at which the filter will raise a log.
        /// \param generator ref to the task generator
        ///
        UnixServer(std::string const& socket_path,
               std::string const& output,
               std::size_t threshold,
               Generator& generator);

        virtual ~UnixServer() = default;

        // Make the UnixServer non copyable & non movable
        UnixServer(UnixServer const&) = delete;

        UnixServer(UnixServer const&&) = delete;

        UnixServer& operator=(UnixServer const&) = delete;

        UnixServer& operator=(UnixServer const&&) = delete;

        void Clean() override;

        void HandleStop(boost::system::error_code const& error, int sig) override;

        void Accept() override;

        void HandleAccept(boost::system::error_code const& e) override;

    private:
        std::string _socket_path; //!< Path to the UNIX socket to listen on.
        boost::asio::local::stream_protocol::acceptor _acceptor; //!< Acceptor for the incoming connections.
        boost::asio::local::stream_protocol::socket _new_connection; //!< Socket used to accept a new connection.
    };
}