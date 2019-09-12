/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <hiredis/hiredis.h>
}

#include <mutex>
#include <thread>
#include <string>

#include "Session.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    // The config file is the Redis UNIX Socket here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);
    bool ConfigRedis(std::string redis_socket_path);

    std::string _redis_socket_path; // Redis UNIX socket path
    std::shared_ptr<darwin::toolkit::RedisManager> _redis_manager = nullptr;

    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};