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

#include "Session.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    static constexpr int PING_INTERVAL = 4;
    static constexpr int MAX_RETRIES = 2;

    // The config file is the Redis UNIX Socket here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

    void KeepConnectionAlive();

private:
    redisContext *_redis_connection = nullptr; // The redis handler
    std::mutex _redis_mutex; // hiredis is not thread safe
    std::thread _send_ping_requests; // necessary to keep connection alive from Redis
    std::string _redis_socket_path; // Redis UNIX socket path
    bool _is_stop; // whether the filter has to be stopped or not
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;

    bool ConnectToRedis();
};