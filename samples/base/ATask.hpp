#pragma once

#include <memory>
#include <boost/asio.hpp>
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "../../toolkit/rapidjson/writer.h"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "ASession.fwd.hpp"
#include "config.hpp"
#include "protocol.h"
#include "Time.hpp"

namespace darwin {

    class ATask : public std::enable_shared_from_this<ATask> {
        public:
            ATask(std::string name, 
                std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                std::mutex& cache_mutex, session_ptr_t session,
                darwin_filter_packet_t& _header,
                rapidjson::Document& _body,
                std::string& _raw_body,
                std::string& _logs,
                std::string& _response_body,
                std::vector<unsigned int>& _certitudes);

            virtual ~ATask() = default;

            // Make the ATask non copyable & non movable
            ATask(ATask const&) = delete;

            ATask(ATask const&&) = delete;

            ATask& operator=(ATask const&) = delete;

            ATask& operator=(ATask const&&) = delete;

            /// Entry point of the execution.
            /// MUST be overloaded.
            /// WARNING This method will be executed by a thread.
            virtual void operator()() = 0;
        
        protected:
            /// Return filter code.
            virtual long GetFilterCode() noexcept = 0;
            
            /// Generate the hash
            virtual xxh::hash64_t GenerateHash();

            /// Set starting time
            void SetStartingTime();

            /// Get total time elapsed since the starting time
            double GetDurationMs();

            /// Save the result to cache
            virtual void SaveToCache(const xxh::hash64_t &hash, unsigned int certitude) const;
            
            /// Get the result from the cache
            virtual bool GetCacheResult(const xxh::hash64_t &hash, unsigned int &certitude);
            
            /// Get the name of the filter
            std::string GetFilterName();

            /// Parse a line in the body.
            /// This function should be implemented in each child,
            /// and should be called between every entry to check validity (no early parsing).
            virtual bool ParseLine(rapidjson::Value &line) = 0;

            /// Parse the body received.
            /// This is the default function, trying to get a JSON array from the _raw_body,
            /// if you wan't to recover something else (full/complex JSON, custom data),
            /// override the function in the child class.
            virtual bool ParseBody() ;

        private: 
            std::string _filter_name; //!< name of the filter

        protected:
            //!< Cache received from the Generator
            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
            std::mutex& _cache_mutex;

            std::chrono::time_point<std::chrono::high_resolution_clock> _starting_time;

            bool _is_cache = false;

            session_ptr_t _s; //!< associated session

            darwin_filter_packet_t& _header; //!< Ref to the Header received from the session.
            rapidjson::Document& _body; //!< Ref to the Body received from session (if any).
            std::string& _raw_body; //!< Ref to the Body received from session (if any), that will not be parsed.
            std::string& _logs; //!< Ref to the data given in the logs by the Session
            std::string& _response_body; //!< Ref to the body to send back to the client
            std::vector<unsigned int>& _certitudes; //!< Ref to the Darwin results obtained.

            std::size_t _threshold; //!< Threshold, overriden at creation by the session

    };
    
}