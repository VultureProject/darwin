#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <thread>
#include "ASession.hpp"
#include "Manager.hpp"
#include "Generator.hpp"
namespace darwin {

    class AServer {
        public:
            /// Create an async UNIX stream socket server.
            /// The server runs on nb_threads thread.
            ///
            /// \param socket_path Path of the UNIX socket to listen on.
            /// \param output Filters' output type
            /// \param next_filter_socket Path of the UNIX socket of the filter to send data to.
            /// \param threshold Threshold at which the filter will raise a log.
            // AServer() = default;

            virtual ~AServer() = default;

            // Make the server non copyable & non movable
            AServer(AServer const&) = delete;

            AServer(AServer const&&) = delete;

            AServer& operator=(AServer const&) = delete;

            AServer& operator=(AServer const&&) = delete;

        public:
            /// Start the server and the threads.
            void Run();

            /// Clean the server's ressources (sessions, socket)
            virtual void Clean() = 0;

            boost::asio::io_context &GetIOContext();

        protected:
            
            AServer(std::string const& output,
               std::size_t threshold,
               Generator& generator);
            
            /// Start async waiting for the stopping signals.
            void AwaitStop();

            /// Handler called when signal is received by the async wait.
            ///
            /// \param sig The received signal.
            virtual void HandleStop(boost::system::error_code const& error, int sig) = 0;

            /// Start an async connection acceptation on _new_session's socket.
            virtual void Accept() = 0;

            /// Handler called on async accept trigger.
            virtual void HandleAccept(boost::system::error_code const& e) = 0;

            void InitSignalsAndStart();

        protected:
            std::string _output; //!< Filter's output type
            boost::asio::io_context _io_context; //!< The async io context.
            std::size_t _threshold; //!< Filter's threshold
            boost::asio::signal_set _signals; //!< Set of the stopping signals.
            Generator& _generator; //!< Generator used to create new Sessions.
            Manager _manager; //!< Server's session manager.
        

    };
}