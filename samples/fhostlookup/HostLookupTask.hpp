/// \file     HostLookup.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "protocol.h"
#include "Session.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"

#define DARWIN_FILTER_HOSTLOOKUP 0x66726570

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class HostLookupTask : public darwin::Session {
public:
    explicit HostLookupTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            tsl::hopscotch_map<std::string, int>& db);

    ~HostLookupTask() override = default;

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

    /// Read a struct in_addr from the session and
    /// lookup in the bad host map to fill _result.
    ///
    /// \return the certitude of host's bad reputation (100: BAD, 0:Good)
    unsigned int DBLookup(const std::string &host) noexcept;

    /// Parse the body received.
    bool ParseBody() override;

private:
    tsl::hopscotch_map<std::string, int> _database ; //The "bad" hostname database
    std::string _current_host; //The host to lookup
    std::vector<std::string> _hosts;
};
