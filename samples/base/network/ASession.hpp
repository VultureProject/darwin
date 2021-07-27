///
/// \file ASession.hpp
/// \author Thibaud Cartegnie (thibaud.cartegnie@advens.fr)
/// \brief Abstract Session to be extended for different protocols
///        It is created by Servers and managed by a Manager
/// \version 1.0
/// \date 20-07-2021
/// 
/// @copyright Copyright (c) 2021
/// 
///
#pragma once

#include <memory>
#include "config.hpp"
#include "Generator.hpp"
#include "Manager.hpp"
#include "DarwinPacket.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "Time.hpp"

#include "ASession.fwd.hpp"

#define DARWIN_SESSION_BUFFER_SIZE 2048
#define DARWIN_DEFAULT_THRESHOLD 80
#define DARWIN_ERROR_RETURN 101

namespace darwin {

    class ASession : public std::enable_shared_from_this<ASession> {
    public:
        ///
        /// \brief Construct a new ASession object
        /// 
        /// \param manager reference to the manager that will manage this session
        /// \param generator referece to the task generator
        ///
        ASession(Manager& manager,
                Generator& generator);

        virtual ~ASession() = default;

        // Make the Session non copyable & non movable
        ASession(ASession const&) = delete;

        ASession(ASession const&&) = delete;

        ASession& operator=(ASession const&) = delete;

        ASession& operator=(ASession const&&) = delete;

    public:
        /// Start the session and the async read of the incoming packet.
        virtual void Start();

        /// Stop the session and close the socket.
        virtual void Stop() = 0;

        /// Set the filter's threshold
        ///
        /// \param threshold The threshold wanted.
        virtual void SetThreshold(std::size_t const& threshold) final;

        /// Set the output's type of the filter
        ///
        /// \param name the string that represent the output type
        virtual void SetOutputType(std::string const& output) final;

        /// Get the filter's output type
        ///
        /// \return The filter's output type
        config::output_type GetOutputType();

        /// Get the filter's result in a log form
        ///
        /// \return The filter's log
        virtual std::string GetLogs();

        /// Get the filter's threshold
        ///
        /// \return The filter's threshold
        size_t GetThreshold();

        /// Send
        virtual void SendNext(DarwinPacket& packet) final;

    protected:
        

        /// Get the data to send to the next filter
        /// according to the filter's output type
        ///
        /// \param size data's size
        /// \param data data to send
        std::string GetDataToSendToFilter();

        virtual void WriteToClient(std::vector<unsigned char>& packet) = 0;

        /// Send result to the client.
        ///
        /// \return false if the function could not send the data, true otherwise.
        virtual bool SendToClient(DarwinPacket& packet) noexcept;

        /// Send result to next filter.
        ///
        /// \return false if the function could not send the data, true otherwise.
        virtual bool SendToFilter(DarwinPacket& packet) noexcept;

        /// Called when data is sent using Send() method.
        /// Terminate the session on failure.
        ///
        /// \param size The number of byte sent.
        virtual void
        SendToClientCallback(const boost::system::error_code& e,
                     std::size_t size);

        std::string JsonStringify(rapidjson::Document &json);

        /// Set the async read for the header.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadHeader() = 0;

        /// Callback of async read for the header.
        /// Terminate the session on failure.
        virtual void ReadHeaderCallback(const boost::system::error_code& e, std::size_t size) final;

        /// Set the async read for the body.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadBody(std::size_t size) = 0;

        /// Callback of async read for the body.
        /// Terminate the session on failure.
        virtual void
        ReadBodyCallback(const boost::system::error_code& e,
                         std::size_t size) final;

        /// Execute the filter and
        virtual void ExecuteFilter();

        /// Sends a response with a body containing an error message
        ///
        /// \param message The error message to send
        /// \param code The error code to send
        virtual void SendErrorResponse(const std::string& message, const unsigned int code) final;

        friend std::shared_ptr<darwin::ATask> Generator::CreateTask(std::shared_ptr<ASession> s) noexcept;

        friend void ATask::run();

        // Not accessible by children
    private:
        std::string _filter_name; //!< name of the filter
        config::output_type _output; //!< The filter's output.

        std::size_t _threshold = DARWIN_DEFAULT_THRESHOLD;

        // Accessible by children
    protected:
        darwin_filter_packet_t _header; //!< Reading Header
        std::array<char, DARWIN_SESSION_BUFFER_SIZE> _body_buffer; //!< Reading buffer for the body.
        Manager& _manager; //!< The associated connection manager.
        Generator& _generator; //!< The Task Generator.
        DarwinPacket _packet;
        std::string _logs; //!< Represents data given in the logs by the Session
    };
}

