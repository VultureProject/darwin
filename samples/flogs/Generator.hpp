/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>

#include "Session.hpp"
#include "../../toolkit/FileManager.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"

class Generator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    // The config file is the database here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);
    bool ConfigRedis(std::string redis_socket_path);

    bool _log{}; // If the filter will stock the data in a log file
    bool _redis{}; // If the filter will stock the data in a REDIS
    std::string _log_file_path = "";
    std::string _redis_list_name = "";
    std::shared_ptr<darwin::toolkit::RedisManager> _redis_manager = nullptr;
    std::shared_ptr<darwin::toolkit::FileManager> _log_file = nullptr;
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache; // The cache for already processed request
};