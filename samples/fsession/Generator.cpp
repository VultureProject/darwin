/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <chrono>
#include <locale>
#include <thread>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "SessionTask.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Loading configuration...");

    std::string redis_socket_path;

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    redis_socket_path = configuration["redis_socket_path"].GetString();
    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    return redis.SetUnixPath(redis_socket_path);
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<SessionTask>(socket, manager, _cache, _cache_mutex));
}
