/// \file     SessionTask.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <hiredis/hiredis.h>
}

#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <any>

#include "protocol.h"
#include "Session.hpp"
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
                         std::mutex& cache_mutex);
    ~SessionTask() override = default;


public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// Reset the expiration of key(s) in Redis depending on cases
    /// will reset the expiration of key(s) <token>_<repo_id> with _expiration
    /// will reset the expiration of the key <token> with _expiration if current TTL is lower
    ///
    /// \return true on success, false otherwise
    bool REDISResetExpire(const std::string &token, const std::string &repo_ids);

    /// Read a session number (from Cookie or HTTP header) from the session and
    /// perform a redis lookup.
    ///
    /// \return true on success, false otherwise.
    unsigned int REDISLookup(const std::string &token, const std::vector<std::string> &repo_ids) noexcept;

    /// Parse a line of the body.
    bool ParseLine(rapidjson::Value &line) final;

private:
    // Session_status in Redis
    std::string _token; // The token to check
    std::vector<std::string> _repo_ids; // The associated repository IDs to check
    uint64_t _expiration = 0; // The expiration to set
};
