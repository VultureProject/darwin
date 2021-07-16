#pragma once

#include "ASession.hpp"
#include "Generator.hpp"

namespace darwin {

    class UdpSession: public ASession {
    public:
        UdpSession(const udp_buffer_t& buffer,
            Manager& manager,
            Generator& generator);

        ~UdpSession() = default;

        void Stop() override final;

        void SetNextFilterPort(int port);

        /// Set the async read for the body.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadBody(std::size_t size) override;

    protected:

        /// Set the async read for the header.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadHeader() override;

        /// 
        virtual void WriteToClient(std::vector<unsigned char>& packet) override;

        virtual void CloseFilterConnection() override;
        virtual void ExecuteFilter() override;
    private:

        const udp_buffer_t& _buffer;
    };

}