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

// The config file is the Redis UNIX Socket here
bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Session:: Generator:: Configuring...");

    if (!SetUpClassifier(configFile)) return false;

    // The config file is the Redis UNIX Socket here
    //FIXME: Support redis password
    _redis_connection = redisConnectUnix(_redis_socket_path.c_str());
    _is_stop = false;

    /* Ignore signals for broken pipes. Otherwise, if the Redis UNIX socket does not exist anymore, this filter will crash */
    signal(SIGPIPE, SIG_IGN);

    if (!ConnectToRedis()) return false;

    /* To prevent the filter from being disconnected from REDIS */
    KeepConnectionAlive();

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    /* Parse config file to fill-in attribute conf of this */
    DARWIN_LOG_DEBUG("Session:: Generator:: Configured");
    return true;
}

bool Generator::SetUpClassifier(const std::string &configuration_file_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Setting up classifier...");
    DARWIN_LOG_DEBUG("Session:: Generator:: Parsing configuration from \"" + configuration_file_path + "\"...");

    std::ifstream conf_file_stream;
    conf_file_stream.open(configuration_file_path, std::ifstream::in);

    if (!conf_file_stream.is_open()) {
        DARWIN_LOG_ERROR("Session:: Generator:: Could not open the configuration file");

        return false;
    }

    std::string raw_configuration((std::istreambuf_iterator<char>(conf_file_stream)),
                                  (std::istreambuf_iterator<char>()));

    rapidjson::Document configuration;
    configuration.Parse(raw_configuration.c_str());

    DARWIN_LOG_DEBUG("Session:: Generator:: Reading configuration...");

    if (!LoadClassifier(configuration)) {
        return false;
    }

    conf_file_stream.close();

    return true;
}

bool Generator::LoadClassifier(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Session:: Generator:: Loading classifier...");

    if (!configuration.HasMember("redis_socket_path")) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: Missing parameter: \"redis_socket_path\"");
        return false;
    }

    if (!configuration["redis_socket_path"].IsString()) {
        DARWIN_LOG_CRITICAL("Session:: Generator:: \"redis_socket_path\" needs to be a string");
        return false;
    }

    _redis_socket_path = configuration["redis_socket_path"].GetString();
    
    return true;
}

bool Generator::ConnectToRedis() {
    DARWIN_LOGGER;

    if (_redis_connection) {
        DARWIN_LOG_DEBUG("Session:: Generator:: Freeing previous Redis connection");
        redisFree(_redis_connection);
        _redis_connection = nullptr;
    }

    DARWIN_LOG_DEBUG("Session:: Generator:: Connecting to Redis");
    _redis_connection = redisConnectUnix(_redis_socket_path.c_str());

    if (_redis_connection == nullptr || _redis_connection->err != 0) {
        if (_redis_connection) {
            redisFree(_redis_connection);
            _redis_connection = nullptr;
        }

        DARWIN_LOG_CRITICAL("Session:: Generator:: Configure:: Cannot connect to Redis");
        return false;
    }

    DARWIN_LOG_DEBUG("Session:: Generator:: Connected to Redis");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<SessionTask>(socket, manager, _cache, _redis_connection, &_redis_mutex));
}

Generator::~Generator() {
    DARWIN_LOGGER;

    if (_redis_connection) {
        DARWIN_LOG_DEBUG("Generator::~Generator:: Closing Redis connection");

        redisFree(_redis_connection);
        _redis_connection = nullptr;
    } else {
        DARWIN_LOG_DEBUG("Generator::~Generator:: No Redis connection to close");
    }

    _is_stop = true;

    if (_send_ping_requests.joinable()) {
        DARWIN_LOG_DEBUG("Generator::~Generator:: Joining ping requests thread");

        _send_ping_requests.join();
    } else {
        DARWIN_LOG_DEBUG("Generator::~Generator:: No ping requests thread to join");
    }

    DARWIN_LOG_DEBUG("Generator::~Generator:: Done");
}

void Generator::KeepConnectionAlive() {
    DARWIN_LOGGER;

    if (PING_INTERVAL <= 0) {
        DARWIN_LOG_INFO("Generator::KeepConnectionAlive:: Ping interval set to " + std::to_string(PING_INTERVAL) + ". No recurrent ping will be sent");

        return;
    }

    DARWIN_LOG_INFO("Generator::KeepConnectionAlive:: Setting up thread to ping Redis every " + std::to_string(PING_INTERVAL) + " seconds");

    int interval = PING_INTERVAL;

    _send_ping_requests = std::thread([this, interval]() {
        DARWIN_LOGGER;
        redisReply *reply = nullptr;
        bool is_running = true;
        unsigned int retries = 0;

        while (is_running && !_is_stop) {
            _redis_mutex.lock();

            if (_redis_connection) {
                DARWIN_LOG_DEBUG("Generator::KeepConnectionAlive:: Sending ping");
                reply = (redisReply *)redisCommand(_redis_connection, "PING");
            }

            if (!_redis_connection || !reply) {
                ++retries;
                freeReplyObject(reply);
                DARWIN_LOG_CRITICAL("Generator::KeepConnectionAlive:: Reply returned is nullptr. Trying to reconnect to Redis...");
                if (!ConnectToRedis()) {
                    DARWIN_LOG_CRITICAL("Generator::KeepConnectionAlive:: Could NOT reconnect to Redis (" + std::to_string(retries) +
                                        " attempt(s) out of " + std::to_string(MAX_RETRIES) + "). Will try again later");
                } else {
                    DARWIN_LOG_CRITICAL("Generator::KeepConnectionAlive:: Connection to Redis retrieved");
                    retries = 0;
                }

                is_running = (retries <= MAX_RETRIES);

                _redis_mutex.unlock();
                std::this_thread::sleep_for(std::chrono::seconds(interval));
                continue;
            }

            _redis_mutex.unlock();

            if (reply->type != REDIS_REPLY_STATUS || std::string(reply->str) != "PONG") {
                DARWIN_LOG_ERROR("Generator::KeepConnectionAlive:: Bad reply while pinging");
            }

            /* Reply will be always nullptr */
            freeReplyObject(reply);
            reply = nullptr;
            is_running = _redis_connection != nullptr;

            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }

        DARWIN_LOG_DEBUG("Generator::KeepConnectionAlive:: Redis connection is closed");
    });
}
