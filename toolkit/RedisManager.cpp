#include <utility>

#include "RedisManager.hpp"

#include <chrono>
#include <locale>
#include <thread>
#include <signal.h>
#include "base/Logger.hpp"


/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit{

        RedisManager::RedisManager(std::string socket_path)
                : _redis_socket_path{std::move(socket_path)} {}

        bool RedisManager::ConnectToRedis(bool keepAlive) {
            DARWIN_LOGGER;

            /* Ignore signals for broken pipes.
            * Otherwise, if the Redis UNIX socket does not exist anymore,
            * this filter will crash */
            signal(SIGPIPE, SIG_IGN);

            if (_redis_connection) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectToRedis:: Freeing previous Redis connection");
                redisFree(_redis_connection);
                _redis_connection = nullptr;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectToRedis:: Connecting to Redis");
            _redis_connection = redisConnectUnix(_redis_socket_path.c_str());
            _is_stop = false;

            if (_redis_connection == nullptr || _redis_connection->err != 0) {
                if (_redis_connection) {
                    redisFree(_redis_connection);
                    _redis_connection = nullptr;
                }

                DARWIN_LOG_CRITICAL("RedisManager::ConnectToRedis:: Configure:: Cannot connect to Redis");
                return false;
            }

            if(keepAlive) {
                KeepConnectionAlive();
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectToRedis:: Connected to Redis");

            return true;
        }

        void RedisManager::KeepConnectionAlive() {
            DARWIN_LOGGER;

            if (PING_INTERVAL <= 0) {
                DARWIN_LOG_INFO(
                        "RedisManager::KeepConnectionAlive:: Ping interval set to " + std::to_string(PING_INTERVAL) +
                        ". No recurrent ping will be sent");

                return;
            }

            DARWIN_LOG_INFO("RedisManager::KeepConnectionAlive:: Setting up thread to ping Redis every " +
                            std::to_string(PING_INTERVAL) + " seconds");

            int interval = PING_INTERVAL;

            _send_ping_requests = std::thread([this, interval]() {
                DARWIN_LOGGER;
                redisReply *reply = nullptr;
                bool is_running = true;
                unsigned int retries = 0;

                while (is_running && !_is_stop) {
                    _redis_mutex.lock();

                    if (_redis_connection) {
                        DARWIN_LOG_DEBUG("RedisManager::KeepConnectionAlive:: Sending ping");
                        reply = (redisReply *) redisCommand(_redis_connection, "PING");
                    }

                    if (!_redis_connection || !reply) {
                        ++retries;
                        freeReplyObject(reply);
                        DARWIN_LOG_CRITICAL(
                                "RedisManager::KeepConnectionAlive:: Redis gave no reply. Trying to reconnect to Redis...");
                        if (!ConnectToRedis()) {
                            DARWIN_LOG_CRITICAL("RedisManager::KeepConnectionAlive:: Could NOT reconnect to Redis (" +
                                                std::to_string(retries) +
                                                " attempt(s) out of " + std::to_string(MAX_RETRIES) +
                                                "). Will try again later");
                        } else {
                            DARWIN_LOG_CRITICAL("RedisManager::KeepConnectionAlive:: Connection to Redis retrieved");
                            retries = 0;
                        }

                        is_running = (retries <= MAX_RETRIES);

                        _redis_mutex.unlock();
                        std::this_thread::sleep_for(std::chrono::seconds(interval));
                        continue;
                    }

                    _redis_mutex.unlock();

                    if (reply->type != REDIS_REPLY_STATUS || std::string(reply->str) != "PONG") {
                        DARWIN_LOG_ERROR("RedisManager::KeepConnectionAlive:: Bad reply while pinging");
                    }

                    /* Reply will never be nullptr */
                    freeReplyObject(reply);
                    reply = nullptr;
                    is_running = _redis_connection != nullptr;

                    std::this_thread::sleep_for(std::chrono::seconds(interval));
                }

                DARWIN_LOG_DEBUG("RedisManager::KeepConnectionAlive:: Redis connection is closed");
            });
        }

        bool RedisManager::REDISQuery(redisReply **reply_ptr, const std::vector<std::string> &arguments) noexcept {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Querying Redis...");

            if (!_redis_connection) {
                DARWIN_LOG_ERROR("RedisManager::REDISQuery:: Not connected to Redis, cannot execute Query");
                return false;
            }

            int arguments_number = (int) arguments.size();
            std::vector<const char *> c_arguments;

            c_arguments.reserve(arguments.size());
            for (const auto &argument : arguments) {
                c_arguments.push_back(argument.c_str());
            }

            const char **formatted_arguments = &c_arguments[0];

            *reply_ptr = nullptr;
            bool result = false;
            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Locking Redis...");
            _redis_mutex.lock();
            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Redis locked");

            if ((*reply_ptr = (redisReply *) redisCommandArgv(_redis_connection, arguments_number, formatted_arguments,
                                                              nullptr)) != nullptr) {
                if ((*reply_ptr)->type != REDIS_REPLY_ERROR) {
                    result = true;
                } else {
                    DARWIN_LOG_ERROR("RedisManager::REDISQuery:: Error while executing command: " +
                                     std::string((*reply_ptr)->str));
                    freeReplyObject(*reply_ptr);
                    *reply_ptr = nullptr;
                }
            }

            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Unlocking Redis...");
            _redis_mutex.unlock();
            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Redis unlocked");

            return result;
        }

        RedisManager::~RedisManager() {
            DARWIN_LOGGER;

            if (_redis_connection) {
                DARWIN_LOG_DEBUG("RedisManager::~RedisManager:: Closing Redis connection");

                redisFree(_redis_connection);
                _redis_connection = nullptr;
            } else {
                DARWIN_LOG_DEBUG("RedisManager::~RedisManager:: No Redis connection to close");
            }

            _is_stop = true;

            if (_send_ping_requests.joinable()) {
                DARWIN_LOG_DEBUG("RedisManager::~RedisManager:: Joining ping requests thread ");

                _send_ping_requests.join();
            } else {
                DARWIN_LOG_DEBUG("RedisManager::~RedisManager:: No ping requests thread to join");
            }

            DARWIN_LOG_DEBUG("RedisManager::~RedisManager:: Done");
        }

    }
}
