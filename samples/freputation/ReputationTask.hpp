/// \file     ReputationTask.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     10/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <maxminddb.h>
#include <unordered_set>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"
#include "protocol.h"
#include "Session.hpp"


#define DARWIN_FILTER_REPUTATION 0x72657075

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class ReputationTask : public darwin::Session {
public:
    explicit ReputationTask(boost::asio::local::stream_protocol::socket& socket,
                       darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                       MMDB_s* db);
    ~ReputationTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Get the result from the cache
    xxh::hash64_t GenerateHash() override;
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
    bool ReadFromSession(const std::string &ip_address, MMDB_lookup_result_s &result) noexcept;

    /// Read a struct in_addr from the session and
    /// lookup in the mmdb to fill _result.
    ///
    /// \return true on success, false otherwise.
    bool DBLookupIn(const std::string &ip_address, MMDB_lookup_result_s &result) noexcept;

    /// Read a struct in6_addr from the session and
    /// lookup in the mmdb to fill _result.
    ///
    /// \return true on success, false otherwise.
    bool DBLookupIn6(const std::string &ip_address, MMDB_lookup_result_s &result) noexcept;

    /// Get the reputation from the results.
    ///
    /// \return true on success, false otherwise.
    unsigned int GetReputation(const std::string &ip_address) noexcept;

    /// Parse a line in the body.
    bool ParseLine(rapidjson::Value &line) final;

private:
    MMDB_s* _database; // The Reputation database
    std::string _ip_address; // The IP address to check
    std::unordered_set<std::string> _tags; // The tags used to check the reputation
};
