/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "Session.hpp"
#include "data_pool.hpp"
#include "ContentInspectionTask.hpp"


class Generator {
public:
    Generator();
    ~Generator();

public:
    // The config file is the database here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
    Configurations _configurations;
    MemManagerParams *_memoryManager;
};