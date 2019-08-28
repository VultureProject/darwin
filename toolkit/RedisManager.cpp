#include <utility>

#include "RedisManager.hpp"

#include <chrono>
#include <locale>
#include <thread>
#include <signal.h>
#include <string.h>
#include <stdio.h>
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

            if(!ConnectToRedisMaster()){
                DARWIN_LOG_CRITICAL("RedisManager::ConnectToRedis:: Configure:: Cannot connect to Redis master");
                return false;
            }

            if(keepAlive) {
                KeepConnectionAlive();
            }
            
            DARWIN_LOG_DEBUG("RedisManager::ConnectToRedis:: Connected to Redis");
            return true;
        }

        bool RedisManager::ConnectToRedisMaster() {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::ConnectToRedisMaster:: Begin...");

            std::string master_ip;
            long long int master_port;
            int is_master_res;

            is_master_res = IsMaster(master_ip, master_port);
            if (is_master_res == 0) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectToRedisMaster:: Connection was slave, now connect to master : "
                                 + master_ip + ", " + std::to_string(master_port));
                return ConnectWithIp(master_ip , master_port);
            } else if (is_master_res == 1) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectToRedisMaster:: Already master");
                return true;
            } else {
                DARWIN_LOG_ERROR("RedisManager::ConnectToRedisMaster:: "
                                 "Error when trying to be master");
                return false;
            }
        }

        bool RedisManager::ConnectWithIp(std::string ip, int port){
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::ConnectWithIp:: Begin...");

            DARWIN_LOG_DEBUG("RedisManager::ConnectWithIp:: Connection to host " + ip +
                             " and port " + std::to_string(port));

            _redis_connection = redisConnect(ip.c_str(), port);

            if (_redis_connection == nullptr || _redis_connection->err) {
                if (_redis_connection) {
                    redisFree(_redis_connection);
                }
                DARWIN_LOG_CRITICAL("RedisManager::ConnectWithIp:: "
                                 "Error when trying to connect");
                return false;
            }
            return true;
        }

        int RedisManager::IsMaster(std::string &masterIp, long long int &masterPort){
            DARWIN_LOGGER;
            DARWIN_LOG_ERROR("RedisManager::IsMaster:: Begin...");

            redisReply *reply = nullptr;

            std::vector<std::string> arguments;
            arguments.emplace_back("ROLE");

            if (!REDISQuery(&reply, arguments)) {
                DARWIN_LOG_ERROR("RedisManager::IsMaster:: Something went wrong while querying Redis' role");
                freeReplyObject(reply);
                return -1;
            }

            if (!reply || reply->type != REDIS_REPLY_ARRAY) {
                DARWIN_LOG_ERROR("RedisManager::IsMaster:: Not the expected Redis response");
                freeReplyObject(reply);
                return -1;
            }

            if(strcmp("slave", reply->element[0]->str) == 0){
                masterIp = reply->element[1]->str;
                masterPort = reply->element[2]->integer;
                freeReplyObject(reply);
                return 0;
            } else if (strcmp("master", reply->element[0]->str) == 0){
                freeReplyObject(reply);
                return 1;
            }

            freeReplyObject(reply);
            return -1;
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

        bool RedisManager::REDISSendArgs(redisReply **reply_ptr,
                                         const char **formatted_arguments,
                                         int arguments_number){
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::REDISSendArgs:: Locking Redis...");
            _redis_mutex.lock();
            DARWIN_LOG_DEBUG("RedisManager::REDISSendArgs:: Redis locked");
            *reply_ptr  = (redisReply *) redisCommandArgv(_redis_connection,
                                                          arguments_number,
                                                          formatted_arguments,
                                                          nullptr);
            DARWIN_LOG_DEBUG("RedisManager::REDISSendArgs:: Unlocking Redis...");
            _redis_mutex.unlock();
            DARWIN_LOG_DEBUG("RedisManager::REDISSendArgs:: Redis unlocked");
            return *reply_ptr != nullptr;
        }


        bool RedisManager::REDISQuery(redisReply **reply_ptr,
                                      const std::vector<std::string> &arguments) noexcept {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Querying Redis...");
            unsigned int retry = 0;

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

            while(retry<=MAX_QUERY_RETRIES){
                *reply_ptr = nullptr;
                if (REDISSendArgs(reply_ptr, &c_arguments[0], arguments_number)){
                    if ((*reply_ptr)->type != REDIS_REPLY_ERROR) {
                        // Everything goes as planned
                        return true;
                    } else {
                        // If we are on a readonly slave we continue to retry
                        if(strncmp("READONLY", (*reply_ptr)->str, 8) == 0){
                            DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Trying to write on a readonly slave, "
                                             "connect to master and retrying the request");
                            if(!ConnectToRedisMaster()){
                                freeReplyObject(*reply_ptr);
                                *reply_ptr = nullptr;
                                return false;
                            }
                            retry++;
                            continue;
                        } else {
                            // Else we stop it
                            DARWIN_LOG_ERROR("RedisManager::REDISQuery:: Error while executing command: " +
                                             std::string((*reply_ptr)->str));
                            freeReplyObject(*reply_ptr);
                            *reply_ptr = nullptr;
                            return false;
                        }
                    }
                }
                DARWIN_LOG_DEBUG("RedisManager::REDISQuery:: Redis not responding, "
                                 "retrying the request");
                retry++;
            }

            if(retry > MAX_QUERY_RETRIES){
                DARWIN_LOG_ERROR("RedisManager::REDISQuery:: Retrying the query too many times ("
                                 + std::to_string(MAX_QUERY_RETRIES) +"), stop retrying");
                freeReplyObject(*reply_ptr);
                *reply_ptr = nullptr;
                return false;
            }
            return false;
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
