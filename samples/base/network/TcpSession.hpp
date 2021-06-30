#pragma once

#include "ASession.hpp"
#include "Generator.hpp"

namespace darwin {

    class TcpSession: public ASession {
    public:
        TcpSession(boost::asio::ip::tcp::socket& socket,
            Manager& manager,
            Generator& generator);

        ~TcpSession() = default;

        void Stop() override final;

        void SetNextFilterPort(int port);

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

        boost::asio::ip::tcp::socket _socket; //!< Session's socket.
        boost::asio::ip::tcp::socket _filter_socket; //!< Filter's socket.

        int _next_filter_port;
        
    };

}