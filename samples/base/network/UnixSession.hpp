#pragma once

#include "ASession.hpp"

namespace darwin {

    class UnixSession: public ASession {
    public:
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

        virtual void CloseFilterConnection() override final;

    private:

        bool _connected; //!< True if the socket to the next filter is connected.

        boost::asio::local::stream_protocol::socket _socket; //!< Session's socket.
        boost::asio::local::stream_protocol::socket _filter_socket; //!< Filter's socket.
        std::string _next_filter_path; //!< The socket path to the next filter.

    };

}