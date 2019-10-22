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
#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/RedisManager.hpp"


#define DARWIN_FILTER_SESSION 0x73657373


// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class SessionTask : public darwin::Session {
public:
    explicit SessionTask(boost::asio::local::stream_protocol::socket& socket,
                       darwin::Manager& manager,
                         std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache);
    ~SessionTask() override = default;


public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

    // The maximum length of the session key (SHA-256 = 64 chars)
    static constexpr unsigned int TOKEN_SIZE = (2*SHA256_DIGEST_LENGTH);

protected:
    /// Get the result from the cache
    virtual xxh::hash64_t GenerateHash() override;
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Read header from the session and
    /// call the method appropriate to the data type received.
    ///
    /// \return true on success, false otherwise.
    bool ReadFromSession(const std::string &token, const std::vector<std::string> &repo_ids) noexcept;

    /// Read a session number (from Cookie or HTTP header) from the session and
    /// perform a redis lookup.
    ///
    /// \return true on success, false otherwise.
    unsigned int REDISLookup(const std::string &token, const std::vector<std::string> &repo_ids) noexcept;

    /// Concatenates our repository IDs into a string, separated with spaces.
    ///
    /// \return The concatenated string.
    std::string JoinRepoIDs(const std::vector<std::string> &repo_ids);

    /// Parse the body received.
    bool ParseBody() override;

private:
    // Session_status in Redis
    std::string _current_token; // The token to check
    std::vector<std::string> _tokens;
    std::vector<std::string> _current_repo_ids; // The associated repository IDs to check
    std::vector<std::vector<std::string>> _repo_ids_list;
};
