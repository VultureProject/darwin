/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     10/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <maxminddb.h>
}

#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    // The config file is the database here
    bool Configure(std::string const& configFile, const std::size_t cache_size);
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    MMDB_s _database; // The GeoIP database
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};