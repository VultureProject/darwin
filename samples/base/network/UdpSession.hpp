///
/// \file UdpSession.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Session for UDP Protocol
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

    class UdpSession: public ASession {
    public:
        ///
        /// \brief Construct a new Udp Session object
        /// 
        /// \param buffer ref to a buffer containing a udp datagram
        /// \param manager ref to the sessions' manager
        /// \param generator ref to the task generator
        ///
        UdpSession(const udp_buffer_t& buffer,
            Manager& manager,
            Generator& generator);

        ~UdpSession() = default;

        void Stop() override final;

        void SetNextFilterPort(int port);

        /// Set the async read for the body.
        ///
        virtual void ReadBody(std::size_t size) override;

    protected:

        /// Set the async read for the header.
        ///
        virtual void ReadHeader() override;

        virtual void WriteToClient(std::vector<unsigned char>& packet) override;

    private:

        const udp_buffer_t& _buffer;
    };

}