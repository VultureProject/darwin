/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/RedisManager.hpp"
#include "Session.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);
    bool ConfigRedis(const std::string &redis_socket_path, const std::string &init_data_path);

    unsigned int _redis_expire = 0;
    std::shared_ptr<darwin::toolkit::RedisManager> _redis_manager = nullptr;
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};