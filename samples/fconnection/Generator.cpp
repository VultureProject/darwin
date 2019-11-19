/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "Generator.hpp"
#include "base/Logger.hpp"
#include "ConnectionSupervisionTask.hpp"

#include <regex>
#include <string>
#include <fstream>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/RedisManager.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Loading classifier...");

    std::string redis_socket_path;
    std::string init_data_path;

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("ConnectionSupervision:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("ConnectionSupervision:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    redis_socket_path = configuration["redis_socket_path"].GetString();

    if (configuration.HasMember("init_data_path")) {
        if (!configuration["init_data_path"].IsString()) {
            DARWIN_LOG_CRITICAL("ConnectionSupervision:: Generator:: \"init_data_path\" needs to be a string");
            return false;
        }
        init_data_path = configuration["init_data_path"].GetString();
    }

    if (!configuration.HasMember("redis_expire")) {
        _redis_expire = 0;
        DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: No redis expire set");
    } else {
        if (!configuration["redis_expire"].IsUint()) {
            DARWIN_LOG_CRITICAL("ConnectionSupervision:: Generator:: \"redis_expire\" needs to be an unsigned int");
            return false;
        }

        _redis_expire = configuration["redis_expire"].GetUint();
    }

    return ConfigRedis(redis_socket_path, init_data_path);
}

bool Generator::ConfigRedis(const std::string &redis_socket_path, const std::string &init_data_path) {
    DARWIN_LOGGER;

    std::ifstream init_data_stream;
    std::string current_line;
    std::vector<std::string> arguments;

    darwin::toolkit::RedisManager& redis = darwin::toolkit::RedisManager::GetInstance();
    if(not redis.SetUnixPath(redis_socket_path)) {
        DARWIN_LOG_ERROR("ConnectionSupervision::Generator::ConfigureRedis:: Could not configure Redis connection.");
        return false;
    }

    if(init_data_path.empty()) return true;

    DARWIN_LOG_DEBUG("ConnectionSupervision:: ConfigRedis:: Loading initial "
                     "data from \"" + init_data_path + "\" for redis...");

    init_data_stream.open(init_data_path, std::ifstream::in);

    if (!init_data_stream.is_open()) {
        DARWIN_LOG_ERROR("ConnectionSupervision:: ConfigRedis:: Could not open the initial data for redis file");
        return false;
    }

    // For each line in the file, we APPEND as a key in the redis, with a whatever value
    while (!init_data_stream.eof() && std::getline(init_data_stream, current_line)) {

        if (!std::regex_match (current_line, std::regex(
                "(((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\."
                "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?);){2})"
                "(([0-9]+;(17|6))|([0-9]*;*1))")))
        {
            DARWIN_LOG_WARNING("ConnectionSupervision:: ParseLogs:: The data: "+ current_line +", isn't valid, ignored. "
                                                                               "Format expected : "
                                                                               "[\"[ip4]\";\"[ip4]\";((\"[port]\";"
                                                                               "\"[ip_protocol udp or tcp]\")|"
                                                                               "\"[ip_protocol icmp]\")]");
            continue;
        }

        arguments.clear();
        if(_redis_expire){
            arguments.emplace_back("SETEX");
            arguments.emplace_back(current_line);
            arguments.emplace_back(std::to_string(_redis_expire));
        } else{
            arguments.emplace_back("SET");
            arguments.emplace_back(current_line);
        }
        arguments.emplace_back("0");

        if (redis.Query(arguments) == REDIS_REPLY_ERROR) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask::ConfigRedis:: "
                             "Error when trying to add line \"" + current_line + "\" from initial data for redis, line "
                                                                                 "not added");
        }
    }

    init_data_stream.close();
    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ConfigRedis:: Initial data loaded in redis");
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<ConnectionSupervisionTask>(socket, manager, _cache, _cache_mutex, _redis_expire));
}

Generator::~Generator() = default;
