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


bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("ConnectionSupervision:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Loading classifier...");

    std::string redis_socket_path;
    std::string init_data_path = "";

    if (!configuration.IsObject()) {
        DARWIN_LOG_CRITICAL("ConnectionSupervision:: Generator:: Configuration is not a JSON object");
        return false;
    }

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

    redisReply *reply = nullptr;
    std::vector<std::string> arguments;

    _redis_manager = std::make_shared<darwin::toolkit::RedisManager>(redis_socket_path);

    /* Ignore signals for broken pipes.
     * Otherwise, if the Redis UNIX socket does not exist anymore,
     * this filter will crash */
    signal(SIGPIPE, SIG_IGN);

    if (!_redis_manager->ConnectToRedis(true)) return false;

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

        if (!_redis_manager->REDISQuery(&reply, arguments)) {
            DARWIN_LOG_ERROR("ConnectionSupervisionTask::ConfigRedis:: "
                             "Error when trying to add line \"" + current_line + "\" from initial data for redis, line "
                                                                                 "not added");
        }
        freeReplyObject(reply);
        reply = nullptr;
    }

    init_data_stream.close();

    DARWIN_LOG_DEBUG("ConnectionSupervisionTask:: ConfigRedis:: Initial data loaded in redis");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<ConnectionSupervisionTask>(socket, manager, _cache, _redis_manager, _redis_expire));
}

Generator::~Generator() = default;
