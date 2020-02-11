/// \file     Session.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     05/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <memory>
#include <boost/asio.hpp>

#include "config.hpp"
#include "protocol.h"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "Time.hpp"

#define DARWIN_SESSION_BUFFER_SIZE 2048
#define DARWIN_DEFAULT_THRESHOLD 80
#define DARWIN_ERROR_RETURN 101

namespace darwin {

    class Manager;

    class Session : public std::enable_shared_from_this<Session> {
    public:
        Session(std::string name,
                boost::asio::local::stream_protocol::socket& socket,
                Manager& manager,
                std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                std::mutex& cache_mutex);

        virtual ~Session() = default;

        // Make the manager non copyable & non movable
        Session(Session const&) = delete;

        Session(Session const&&) = delete;

        Session& operator=(Session const&) = delete;

        Session& operator=(Session const&&) = delete;

    public:
        /// Entry point of the execution.
        /// MUST be overloaded.
        /// WARNING This method will be executed by a thread.
        virtual void operator()() = 0;

    public:
        /// Start the session and the async read of the incoming packet.
        virtual void Start() final;

        /// Stop the session and close the socket.
        virtual void Stop() final;

        /// Set the filter's threshold
        ///
        /// \param threshold The threshold wanted.
        virtual void SetThreshold(std::size_t const& threshold) final;

        /// Set the path to the associated decision module UNIX socket
        ///
        /// \param path Path to the UNIX socket.
        virtual void SetNextFilterSocketPath(std::string const& path) final;

        /// Set the output's type of the filter
        ///
        /// \param name the string that represent the output type
        virtual void SetOutputType(std::string const& output) final;

    protected:
        /// Return filter code.
        virtual long GetFilterCode() noexcept = 0;

        /// Save the result to cache
        virtual void SaveToCache(const xxh::hash64_t &hash, unsigned int certitude) const;

        /// Get the result from the cache
        virtual bool GetCacheResult(const xxh::hash64_t &hash, unsigned int &certitude);

        /// Generate the hash
        virtual xxh::hash64_t GenerateHash();

        /// Set starting time
        void SetStartingTime();

        /// Get total time elapsed since the starting time
        double GetDurationMs();

        /// Get the filter's output type
        ///
        /// \return The filter's output type
        config::output_type GetOutputType();

        /// Get the data to send to the next filter
        /// according to the filter's output type
        ///
        /// \param size data's size
        /// \param data data to send
        std::string GetDataToSendToFilter();

        /// Get the filter's result in a log form
        ///
        /// \return The filter's log
        virtual std::string GetLogs();


        /// Transform the evt id in the header into a string
        ///
        /// \return evt_di as string
        std::string Evt_idToString();

        /// Get the name of the filter
        std::string GetFilterName();

        /// Get the string representation of a rapidjson document
        std::string JsonStringify(rapidjson::Document &json);

        /// Parse the body received.
        /// This is the default function, trying to get a JSON array from the _raw_body,
        /// if you wan't to recover something else (full/complex JSON, custom data),
        /// override the function in the child class.
        virtual bool ParseBody();

        /// Parse a line in the body.
        /// This function should be implemented in each child,
        /// and should be called between every entry to check validity (no early parsing).
        virtual bool ParseLine(rapidjson::Value &line) = 0;

        /// Send
        virtual void SendNext() final;

        /// Send result to the client.
        ///
        /// \return false if the function could not send the data, true otherwise.
        virtual bool SendToClient() noexcept;

        /// Send result to next filter.
        ///
        /// \return false if the function could not send the data, true otherwise.
        virtual bool SendToFilter() noexcept;

        /// Called when data is sent using Send() method.
        /// Terminate the session on failure.
        ///
        /// \param size The number of byte sent.
        virtual void
        SendToClientCallback(const boost::system::error_code& e,
                     std::size_t size);

        /// Called when data is sent using SendToFilter() method.
        /// Terminate the filter session on failure.
        ///
        /// \param size The number of byte sent.
        virtual void
        SendToFilterCallback(const boost::system::error_code& e,
                             std::size_t size);

private:
        /// Set the async read for the header.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadHeader() final;

        /// Callback of async read for the header.
        /// Terminate the session on failure.
        virtual void ReadHeaderCallback(const boost::system::error_code& e, std::size_t size) final;

        /// Set the async read for the body.
        ///
        /// \return -1 on error, 0 on socket closed & sizeof(header) on success.
        virtual void ReadBody(std::size_t size) final;

        /// Callback of async read for the body.
        /// Terminate the session on failure.
        virtual void
        ReadBodyCallback(const boost::system::error_code& e,
                         std::size_t size) final;

        /// Execute the filter and
        virtual void ExecuteFilter() final;

        // Not accessible by children
    private:
        std::string _filter_name; //!< name of the filter
        bool _connected; //!< True if the socket to the next filter is connected.
        std::string _next_filter_path; //!< The socket path to the next filter.
        config::output_type _output; //!< The filter's output.
        std::array<char, DARWIN_SESSION_BUFFER_SIZE> _buffer; //!< Reading buffer for the body.


        // Accessible by children
    protected:
        boost::asio::local::stream_protocol::socket _socket; //!< Session's socket.
        boost::asio::local::stream_protocol::socket _filter_socket; //!< Filter's socket.
        Manager& _manager; //!< The associated connection manager.
        darwin_filter_packet_t _header; //!< Header received from the session.
        rapidjson::Document _body; //!< Body received from session (if any).
        std::string _raw_body; //!< Body received from session (if any), that will not be parsed.
        std::string _logs; //!< Represents data given in the logs by the Session
        std::chrono::time_point<std::chrono::high_resolution_clock> _starting_time;
        std::vector<unsigned int> _certitudes; //!< The Darwin results obtained.
        //!< Cache received from the Generator
        std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
        std::mutex& _cache_mutex;
        bool _is_cache = false;
        std::size_t _threshold = DARWIN_DEFAULT_THRESHOLD; //!<Default threshold
    };

    /// Definition of a session's self-managing pointer.
    ///
    /// \typedef session_ptr_t
    typedef std::shared_ptr<Session> session_ptr_t;

}

