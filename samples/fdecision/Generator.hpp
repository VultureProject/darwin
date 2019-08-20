/// \file     Generator.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     18/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <map>
#include <vector>
#include <mutex>
#include "Session.hpp"
#include "protocol.h"

// The Generator is used to generated configured Task objects.
// You MUST create a class named 'Generator' out of any namespace.

class Generator {
public:
    // The constructor MUST NOT take any parameter.
    Generator() = default;
    ~Generator() = default;

public:
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    std::map<std::string, std::string> data;
    std::mutex data_mutex;
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};
