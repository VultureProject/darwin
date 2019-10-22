
#include "RedisManager.hpp"

#include <chrono>
#include <thread>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "base/Logger.hpp"

namespace darwin {

    namespace toolkit {

        RedisManager& RedisManager::GetInstance() {
            static RedisManager instance;
            return instance;
        }

        RedisManager::RedisManager() {
            _janitorStop = false;
            _janitorRunInterval = JANITOR_RUN_INTERVAL;
            _janitor = std::thread(&RedisManager::JanitorRun, this);
        }

        RedisManager::~RedisManager() {
            _janitorStop = true;
            _janitorCondVar.notify_all();
            _janitor.join();
            for(auto& threadData : _threadSet) {
                if(threadData->_masterContext) redisFree(threadData->_masterContext);
                if(threadData->_slaveContext) redisFree(threadData->_slaveContext);
            }
        }

        void RedisManager::JanitorRun() {
            DARWIN_LOGGER;

            /* Ignore signals for broken pipes.
            * Otherwise, if the Redis UNIX socket does not exist anymore,
            * this filter will crash */
            signal(SIGPIPE, SIG_IGN);

            std::unique_lock<std::mutex> lock(_janitorMut);

            while(not _janitorStop) {
                _janitorCondVar.wait_for(lock, std::chrono::duration<int>(_janitorRunInterval));

                if(_janitorStop) break;

                _setMut.lock();
                DARWIN_LOG_DEBUG("RedisManager::JanitorRun:: Janitor running...");

                for(auto& thread_info : _threadSet) {
                    std::unique_lock<std::mutex> lockMaster(thread_info->_masterContextMut);
                    std::unique_lock<std::mutex> lockSlave(thread_info->_slaveContextMut);

                    if(thread_info->_masterContext) {
                        if(difftime(time(nullptr), thread_info->_masterLastUse) > CONNECTION_DROP_TIMEOUT) {
                            redisFree(thread_info->_masterContext);
                            thread_info->_masterContext = nullptr;
                            DARWIN_LOG_INFO("RedisManager::JanitorRun:: dropping master connection.");
                        }
                        else if(not this->SendPing(thread_info->_masterContext)) {
                            this->Reconnect(thread_info->_masterContext);
                        }
                    }

                    if(thread_info->_slaveContext) {
                        if(difftime(time(nullptr), thread_info->_slaveLastUse) > CONNECTION_DROP_TIMEOUT) {
                            redisFree(thread_info->_slaveContext);
                            thread_info->_slaveContext = nullptr;
                            DARWIN_LOG_INFO("RedisManager::JanitorRun:: dropping slave connection.");
                        }
                        else if(not this->SendPing(thread_info->_slaveContext)) {
                            this->Reconnect(thread_info->_slaveContext);
                        }
                    }
                }

                _setMut.unlock();
                DARWIN_LOG_DEBUG("RedisManager::JanitorRun:: Janitor finished.");
            }
        }

        bool RedisManager::SendPing(redisContext *context) {
            DARWIN_LOGGER;
            redisReply *reply = nullptr;

            if(!context) {
                DARWIN_LOG_ERROR("RedisManager::SendPing:: context is null, cannot ping.");
            }
            std::vector<const char *> arguments {"PING"};
            bool success = this->SendArgs(context, &reply, &arguments[0], arguments.size());

            if(!success) {
                if(reply and reply->type == REDIS_REPLY_ERROR) {
                    DARWIN_LOG_WARNING("RedisManager::SendPing:: Could not send ping -> " + std::string(reply->str, reply->len));
                }
                else {
                    DARWIN_LOG_WARNING("RedisManager::SendPing:: Could not send ping.");
                }
            }

            freeReplyObject(reply);
            return success;
        }

        bool RedisManager::SetUnixPath(const std::string &fullpath) {
            _masterUnixSocket.assign(fullpath);
            return this->Discover();
        }

        bool RedisManager::SetIpAddress(const std::string &ip, const int port) {
            _masterIp.assign(ip);
            _masterPort = port;
            return this->Discover();
        }

        bool RedisManager::SendArgs(redisContext *context,
                                        redisReply **reply_ptr,
                                        const char **formatted_arguments,
                                        int arguments_number){
            *reply_ptr  = (redisReply *) redisCommandArgv(context,
                                                          arguments_number,
                                                          formatted_arguments,
                                                          nullptr);
            return *reply_ptr != nullptr;
        }

        std::shared_ptr<ThreadData> RedisManager::GetThreadInfo() {
            thread_local static std::shared_ptr<ThreadData> instance = std::make_shared<ThreadData>();
            instance->thread_id = std::this_thread::get_id();
            std::unique_lock<std::mutex> lock(_setMut);
            auto result = _threadSet.insert(instance);

            // if the instance was inserted, it was also created -> init a connection
            if(result.second) {
                std::unique_lock<std::mutex> lock(instance->_masterContextMut);
                bool masterSuccess = false;
                if(not _masterUnixSocket.empty()) {
                    masterSuccess = this->ConnectUnixSocket(_masterUnixSocket, &instance->_masterContext);
                }
                else if(not _masterIp.empty()) {
                    masterSuccess = this->ConnectAddress(_masterIp, _masterPort, &instance->_masterContext);
                }

                // If the master connection could not be established, tries to connect to slave straight away
                if(not masterSuccess) {
                    std::unique_lock<std::mutex> lock(instance->_slaveContextMut);
                    if(not _slaveUnixSocket.empty()) {
                        this->ConnectUnixSocket(_slaveUnixSocket, &instance->_slaveContext);
                    }
                    else if(not _slaveIp.empty()) {
                        this->ConnectAddress(_slaveIp, _slavePort, &instance->_slaveContext);
                    }
                }
            }

            return instance;
        }

        bool RedisManager::ConnectUnixSocket(const std::string unixSocket, redisContext **context) {
            DARWIN_LOGGER;
            if(*context) redisFree(*context);

            if(unixSocket.empty()) {
                DARWIN_LOG_ERROR("RedisManager::ConnectUnixSocket:: unix socket path is empty, cannot connect.");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: connecting to Redis socket '" + unixSocket + "'...");
            *context = redisConnectUnix(unixSocket.c_str());

            if(!(*context) || (*context)->err) {
                DARWIN_LOG_ERROR("RedisManager::ConnectUnixSocket:: could not connect to unix socket '" + unixSocket + "'.");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: Connected");
            return true;
        }

        bool RedisManager::ConnectAddress(const std::string masterIp, const int masterPort, redisContext **context) {
            DARWIN_LOGGER;
            if(*context) redisFree(*context);

            if(masterIp.empty() || masterPort == 0) {
                DARWIN_LOG_ERROR("RedisManager::ConnectAddress:: IP and/or port incorrect, cannot connect.");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: connecting to Redis address '" + masterIp + ":" + std::to_string(masterPort) + "'...");
            *context = redisConnect(masterIp.c_str(), masterPort);
            if(!(*context) || (*context)->err) {
                DARWIN_LOG_ERROR("RedisManager::ConnectAddress:: Could not connect to server '" + masterIp + ":" + std::to_string(masterPort) + "'.");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: Connected");
            return true;
        }

        std::string RedisManager::GetRole(redisContext *context, std::mutex& contextMutex, std::string& masterIp, int& masterPort) {
            DARWIN_LOGGER;
            redisReply *reply = nullptr;
            std::string ret("");
            std::unique_lock<std::mutex> lock(contextMutex);

            if(!context) {
                DARWIN_LOG_ERROR("RedisManager::GetRole:: context is null, cannot query Redis.");
                return ret;
            }

            std::vector<const char *> arguments {"ROLE"};
            bool success = this->SendArgs(context, &reply, &arguments[0], arguments.size());

            if(!success) {
                DARWIN_LOG_ERROR("RedisManager::GetRole:: Could not request Redis to get role.");
                return ret;
            }

            if(reply->type != REDIS_REPLY_ARRAY) {
                DARWIN_LOG_ERROR("RedisManager::GetRole:: Did not get expected answer while querying role.");
                freeReplyObject(reply);
                return ret;
            }

            ret = std::string(reply->element[0]->str);

            if(ret == "slave") {
                masterIp.assign(reply->element[1]->str);
                masterPort = reply->element[2]->integer;
            }

            freeReplyObject(reply);
            return ret;
        }

        bool RedisManager::Discover() {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::Discover:: Discovering Redis...");

            // lock _setMut to avoid new thread connections during (potential) changes in parameters
            std::unique_lock<std::mutex> lock(_setMut);

            redisContext *tempContext = nullptr;
            std::mutex tempContextMutex;

            /* Ignore signals for broken pipes.
            * Otherwise, if the Redis UNIX socket does not exist anymore,
            * this filter will crash */
            signal(SIGPIPE, SIG_IGN);

            if(not _masterUnixSocket.empty()) {
                DARWIN_LOG_INFO("RedisManager::Discover:: Connecting to master via unix socket...");
                if (not this->ConnectUnixSocket(_masterUnixSocket, &tempContext)) {
                    return false;
                }
            }
            else if(not _masterIp.empty()) {
                DARWIN_LOG_INFO("RedisManager::Discover:: Connecting to master via IP address...");
                if (not this->ConnectAddress(_masterIp, _masterPort, &tempContext)) {
                    return false;
                }
            }
            else {
                DARWIN_LOG_WARNING("RedisManager::Discover:: Got neither unix socket nor ip address to connect to, aborting.");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::Discover:: Getting role...");
            std::string role = this->GetRole(tempContext, tempContextMutex, _slaveIp, _slavePort);
            if(role.empty()) {
                DARWIN_LOG_ERROR("RedisManager::Discover:: Could not get Redis role. Aborting.");
                if(tempContext) redisFree(tempContext);
                return false;
            }

            if(role == "master") {
                DARWIN_LOG_INFO("RedisManager::Discover:: Successfuly connected to Redis master.");
                if(tempContext) redisFree(tempContext);
                return true;
            }

            // We got a slave connection, attempt to connect to master
            std::swap(_slaveUnixSocket, _masterUnixSocket);
            std::swap(_slaveIp, _masterIp);
            std::swap(_slavePort, _masterPort);
            redisFree(tempContext);
            tempContext = nullptr;
            DARWIN_LOG_INFO("RedisManager::Discover:: Connected to Redis slave, attempting to reach master...");
            if(not this->ConnectAddress(_masterIp, _masterPort, &tempContext)) {
                DARWIN_LOG_WARNING("RedisMangerBis::Discover:: Got slave connection, but could not connect to master.");
                if(tempContext) redisFree(tempContext);
                return true;
            }

            if(tempContext) redisFree(tempContext);
            return true;
        }

        bool RedisManager::Reconnect(redisContext *context) {
            DARWIN_LOGGER;

            if(not context) {
                DARWIN_LOG_ERROR("RedisManager::Reconnect:: No valid connection, aborting.");
                return false;
            }

            int retCode = redisReconnect(context);
            return retCode == REDIS_OK;
        }

        int RedisManager::Query(const std::vector<std::string> &arguments) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object);

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string> &arguments, long long int& reply_int) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object);
            try {
                reply_int = std::any_cast<long long int>(object);
            }
            catch(const std::bad_any_cast& error) {}

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string> &arguments, std::string& reply_string) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object);
            try {
                reply_string.assign(std::any_cast<std::string>(object));
            }
            catch(const std::bad_any_cast& error) {}

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string> &arguments, std::any& reply_object) {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            redisReply *reply = nullptr;
            std::vector<const char *> c_arguments{};
            bool success = false;
            int ret;


            c_arguments.reserve(arguments.size());
            for(const auto &argument : arguments) {
                c_arguments.push_back(argument.c_str());
            }

            std::unique_lock<std::mutex> lockMaster(threadData->_masterContextMut);
            std::unique_lock<std::mutex> lockSlave(threadData->_slaveContextMut);

            // Try to reconnect after connection was closed by janitor
            if(not threadData->_masterContext) {
                if(not _masterUnixSocket.empty()) {
                    this->ConnectUnixSocket(_masterUnixSocket, &threadData->_masterContext);
                }
                else if(not _masterIp.empty()) {
                    this->ConnectAddress(_masterIp, _masterPort, &threadData->_masterContext);
                }
            }

            if(threadData->_masterContext and not threadData->_masterContext->err) {
                threadData->_masterLastUse = time(nullptr);
                success = this->SendArgs(threadData->_masterContext, &reply, &c_arguments[0], c_arguments.size());
            }

            if(not threadData->_masterContext or threadData->_masterContext->err) {
                // Try to use slave connection if we have one
                // And try to reconnect if the current connection is closed (which is likely the case for a fallback)
                if(not threadData->_slaveContext and not _slaveUnixSocket.empty()) {
                    this->ConnectUnixSocket(_slaveUnixSocket, &threadData->_slaveContext);
                }
                else if(not threadData->_slaveContext and not _slaveIp.empty()) {
                    this->ConnectAddress(_slaveIp, _slavePort, &threadData->_slaveContext);
                }

                if(threadData->_slaveContext and not threadData->_slaveContext->err) {
                    threadData->_slaveLastUse = time(nullptr);
                    success = this->SendArgs(threadData->_slaveContext, &reply, &c_arguments[0], c_arguments.size());
                }
                else {
                    DARWIN_LOG_ERROR("RedisManager::Query:: Got no valid connection, cannot query.");
                    return REDIS_REPLY_ERROR;
                }
            }

            if(not reply or not success) {
                DARWIN_LOG_WARNING("RedisManager::Query:: Could not query Redis.");
                return REDIS_REPLY_ERROR;
            }

            this->ParseReply(reply, reply_object);
            ret = reply->type;
            freeReplyObject(reply);
            return ret;
        }

        void RedisManager::ParseReply(redisReply *reply, std::any& reply_object) {
            if(not reply) return;
            DARWIN_LOGGER;

            switch(reply->type) {
                case REDIS_REPLY_ERROR:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseBody:: [string]: " + std::string(reply->str, reply->len));
                    return;
                case REDIS_REPLY_NIL:
                    return;
                case REDIS_REPLY_STATUS:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseBody:: [string]: " + std::string(reply->str, reply->len));
                    return;
                case REDIS_REPLY_INTEGER:
                    reply_object = reply->integer;
                    DARWIN_LOG_DEBUG("RedisManager::ParseBody:: [integer]: " + std::to_string(reply->integer));
                    return;
                case REDIS_REPLY_STRING:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseBody:: [string]: " + std::string(reply->str, reply->len));
                    return;
            }

            DARWIN_LOG_DEBUG("RedisManager::ParseBody:: [[array]]");
            std::vector<std::any> sub_array;
            for(size_t i = 0; i < reply->elements; i++) {
                std::any sub_object;
                this->ParseReply(reply->element[i], sub_object);
                sub_array.push_back(sub_object);
            }
            if(not sub_array.empty()) reply_object = sub_array;
            return;
        }
    }
}
