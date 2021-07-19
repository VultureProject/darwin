/// \file     SessionTask.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <hiredis/hiredis.h>
#include <openssl/sha.h>
}

#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <any>

#include "protocol.h"
#include "Session.hpp"
#include "Generator.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/RedisManager.hpp"


#define DARWIN_FILTER_SESSION 0x73657373
#define DARWIN_FILTER_NAME "session"
#define DARWIN_ALERT_RULE_NAME "Session"
#define DARWIN_ALERT_TAGS "[]"

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class SessionTask : public darwin::Session {
public:
    explicit SessionTask(boost::asio::local::stream_protocol::socket& socket,
                         darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                         std::mutex& cache_mutex,
                         std::unordered_map<std::string, std::unordered_map<std::string, id_timeout>> &applications);
    ~SessionTask() override = default;


public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

    // The maximum length of the session key (SHA-256 = 64 chars)
    static constexpr unsigned int TOKEN_SIZE = (2*SHA256_DIGEST_LENGTH);

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Read header from the session and
    /// call the method appropriate to the data type received.
    ///
    /// \return true on success, false otherwise.
    bool ReadFromSession() noexcept;

    /// Reset the expiration of key(s) in Redis depending on cases
    /// will reset the expiration of key(s) <token>_<repo_id> with _expiration
    /// will reset the expiration of the key <token> with _expiration if current TTL is lower
    ///
    /// \return true on success, false otherwise
    bool REDISResetExpire(const uint64_t expiration);

    /// Read a session number (from Cookie or HTTP header) from the session and
    /// perform a redis lookup.
    ///
    /// \return true on success, false otherwise.
    unsigned int REDISLookup(const id_timeout &id_t) noexcept;

    /// Parse a line of the body.
    bool ParseLine(rapidjson::Value &line) final;

private:
    // Session_status in Redis
    std::string _token; // The token to check
    std::string _domain; // The domain to retrieve
    std::string _path; // The path to retrieve
    std::unordered_map<std::string, std::unordered_map<std::string, id_timeout>> &_applications;
};
