
#include "RedisManager.hpp"

#include <chrono>
#include <thread>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "base/Logger.hpp"

namespace darwin {

    namespace toolkit {

        // ########################################
        // ###        Public Interface          ###
        // ########################################

        RedisManager::~RedisManager() {
            for(auto& threadData : _threadSet) {
                redisFree(threadData->_redisContext);
            }
        }

        RedisManager& RedisManager::GetInstance() {
            static RedisManager instance;
            return instance;
        }


        void RedisManager::SetHealthCheckInterval(const unsigned int interval) {
            this->_healthCheckInterval = interval;
        }


        void RedisManager::SetTimeoutConnect(const unsigned int timeoutSec) {
            this->_connectTimeout = timeoutSec;
        }


        void RedisManager::SetUnixConnection(const std::string& fullpath) {
            std::lock_guard<std::mutex> lock(this->_availableConnectionsMut);
            this->_baseConnection = {
                .socketPath = fullpath,
                .ip = "",
                .port = 0
            };

            return;
        }


        void RedisManager::SetIpConnection(const std::string& ip, const unsigned int port) {
            std::lock_guard<std::mutex> lock(this->_availableConnectionsMut);
            this->_baseConnection = {
                .socketPath = "",
                .ip = ip,
                .port = port
            };

            return;
        }


        bool RedisManager::TestIsMaster(const std::string& unixSocket) {
            DARWIN_LOGGER;
            redisContext *context = nullptr;
            redisReply *reply = nullptr;
            bool ret = false;

            DARWIN_LOG_DEBUG("RedisManager::TestIsMaster:: checking if " + unixSocket + " is a valid redis master");

            if(ConnectUnixSocket(unixSocket, &context)) {
                reply = GetContextRole(context);
                if(reply and strncmp(reply->element[0]->str, "master", 6) == 0) {
                    ret = true;
                }
                freeReplyObject(reply);
                redisFree(context);
            }

            return ret;
        }


        bool RedisManager::TestIsMaster(const std::string& ip, const unsigned int port) {
            DARWIN_LOGGER;
            redisContext *context = nullptr;
            redisReply *reply = nullptr;
            bool ret = false;

            DARWIN_LOG_DEBUG("RedisManager::TestIsMaster:: checking if " + ip + ":"
                                + std::to_string(port)+ " is a valid redis master");

            if(ConnectAddress(ip, port, &context)) {
                reply = GetContextRole(context);
                if(reply and strncmp(reply->element[0]->str, "master", 6)) {
                    ret = true;
                }
                freeReplyObject(reply);
                redisFree(context);
            }

            return ret;
        }


        bool RedisManager::Connect() {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            RedisConnectionInfo connection;
            redisContext *context = nullptr;

            {
                std::lock_guard<std::mutex> lock(this->_availableConnectionsMut);
                if(not this->_activeConnection.isSet() and not this->_baseConnection.isSet()) {
                    DARWIN_LOG_WARNING("RedisManager::Connect:: cannot connect to a redis server, no valid way to connect");
                    return false;
                }

                // Connect either to the base connection if the active connection doesn't exist yet
                // or to the active connection
                connection = this->_activeConnection.isSet() ? this->_activeConnection : this->_baseConnection;
            }

            context = ConnectTo(connection, this->_connectTimeout);

            if(context) {
                if(threadData->_redisContext)
                    redisFree(threadData->_redisContext);
                threadData->_redisContext = context;
            }

            return context != nullptr;
        }


        void RedisManager::Disconnect() {
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            DisconnectContext(&(threadData->_redisContext));
        }


        bool RedisManager::IsMaster() {
            return not this->GetRole().compare("master");
        }


        bool RedisManager::FindAndConnect() {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            redisContext *context = nullptr;

            {
                std::unique_lock<std::mutex> lock(this->_availableConnectionsMut);
                if(not this->Discover()) {
                    DARWIN_LOG_ERROR("RedisManager::FindAndConnect:: could not find valid connection");
                    return false;
                }
                // use newly discovered connection to connect
                context = ConnectTo(this->_activeConnection, this->_connectTimeout);
                if(not context) {
                    DARWIN_LOG_ERROR("RedisManager::FindAndConnect:: could not connect to redis");
                    return false;
                }
                else {
                    if(threadData->_redisContext)
                        redisFree(threadData->_redisContext);
                    threadData->_redisContext = context;
                }
            }

            return true;
        }


        bool RedisManager::FindAndConnectWithRateLimiting() {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();

            // rate limit
            if(std::time(nullptr) > threadData->_lastDiscovery + this->_healthCheckInterval) {
                std::time(&(threadData->_lastDiscovery));
                /*  try first to connect to current active connection (potentially updated),
                    then search master in case of failure */
                if(this->Connect() and this->IsMaster())
                    return true;
                else
                    return this->FindAndConnect();
            }
            else {
                DARWIN_LOG_DEBUG("RedisManager::FindAndConnectWithRateLimiting:: Rate limiting applied");
            }

            return false;
        }


        std::string RedisManager::GetRole() {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            redisReply *reply = nullptr;
            std::string ret;

            reply = GetContextRole(threadData->_redisContext);
            if(reply) {
                std::time(&(threadData->_redisLastUse));
                ret = reply->element[0]->str;
                freeReplyObject(reply);
            }

            DARWIN_LOG_DEBUG("RedisManager::GetRole:: role is " + ret);
            return ret;
        }


        int RedisManager::Query(const std::vector<std::string>& arguments, bool reconnectRetry) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object, reconnectRetry);

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string>& arguments, long long int& reply_int, bool reconnectRetry) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object, reconnectRetry);
            try {
                reply_int = std::any_cast<long long int>(object);
            }
            catch(const std::bad_any_cast& error) {}

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string>& arguments, std::string& reply_string, bool reconnectRetry) {
            int ret;
            std::any object;

            ret = this->Query(arguments, object, reconnectRetry);
            try {
                reply_string.assign(std::any_cast<std::string>(object));
            }
            catch(const std::bad_any_cast& error) {}

            return ret;
        }

        int RedisManager::Query(const std::vector<std::string>& arguments, std::any& reply_object, bool reconnectRetry) {
            DARWIN_LOGGER;
            std::shared_ptr<ThreadData> threadData = this->GetThreadInfo();
            redisReply *reply = nullptr;
            std::vector<const char *> c_arguments{};
            int ret = REDIS_CONNNECTION_ERROR;

            if(not this->HealthCheck(threadData) and not reconnectRetry) {
                DARWIN_LOG_WARNING("RedisManager::Query:: health check failed, aborting");
                return REDIS_CONNNECTION_ERROR;
            }

            // prepare arguments
            c_arguments.reserve(arguments.size());
            for(const auto &argument : arguments) {
                c_arguments.push_back(argument.c_str());
            }

            if(threadData->_redisContext and not threadData->_redisContext->err) {

                SendArgs(threadData->_redisContext, &reply, &c_arguments[0], c_arguments.size());

                if(reply) {
                    std::time(&(threadData->_redisLastUse));
                    ParseReply(reply, reply_object);
                    ret = reply->type;
                    freeReplyObject(reply);
                    reply = nullptr;
                }
                else {
                    DARWIN_LOG_WARNING("RedisManager::Query:: could not query redis");
                    redisFree(threadData->_redisContext);
                    threadData->_redisContext = nullptr;
                }
            }

            /*  ret > REDIS_REPLY_STATUS means there was an error
                3 scenarios:
                - there wasn't a valid context to call
                - redis replied with a logical error -> might be a write attempt on a replica
                - redis didn't reply (connection error) */
            if(ret > REDIS_REPLY_STATUS and reconnectRetry) {
                switch(ret) {
                    case REDIS_REPLY_ERROR:
                        if(this->IsMaster()) {
                            DARWIN_LOG_ERROR("RedisManager::Query:: Master didn't accept command");
                            break;
                        }
                        // will fallthrough if not connected to a Redis Master, to try and find one
                        // nifty C++17 attribute to declare explicit fallthrough (silence complier warnings)
                        [[fallthrough]];
                    case REDIS_CONNNECTION_ERROR:
                        // try to reconnect
                        DARWIN_LOG_INFO("RedisManager::Query:: Trying to find and connect to a Redis Master");
                        if(this->FindAndConnectWithRateLimiting()) {
                            DARWIN_LOG_INFO("RedisManager::Query:: Connected to Master, querying");
                            SendArgs(threadData->_redisContext, &reply, &c_arguments[0], c_arguments.size());
                            if(reply) {
                                ParseReply(reply, reply_object);
                                ret = reply->type;
                                freeReplyObject(reply);
                            }
                            else {
                                DARWIN_LOG_WARNING("RedisManager::Query:: Could not query Redis");
                                redisFree(threadData->_redisContext);
                                threadData->_redisContext = nullptr;
                            }
                        }
                        else {
                            DARWIN_LOG_ERROR("RedisManager::Query:: Could not connect to a valid master");
                            break;
                        }
                }
            }


            return ret;
        }







        // ########################################
        // ###        Private Interface         ###
        // ########################################

        std::shared_ptr<ThreadData> RedisManager::GetThreadInfo() {
            thread_local static std::shared_ptr<ThreadData> threadData = nullptr;
            if(not threadData) {
                threadData = std::make_shared<ThreadData>();
                std::lock_guard<std::mutex> lock(this->_threadSetMut);
                this->_threadSet.insert(threadData);
                signal(SIGPIPE, SIG_IGN);
            }
            return threadData;
        }


        redisReply* RedisManager::GetContextRole(redisContext *context) {
            DARWIN_LOGGER;

            redisReply *reply = nullptr;
            std::vector<const char *> arguments {"ROLE"};
            bool success;

            if(not context) {
                DARWIN_LOG_DEBUG("RedisManager::GetContextRole:: no valid context to call");
                return nullptr;
            }

            success = SendArgs(context, &reply, &arguments[0], arguments.size());

            if(not success) {
                DARWIN_LOG_DEBUG("RedisManager::GetContextRole:: could not request redis to get role");
                redisFree(context);
                context = nullptr;
                return nullptr;
            }

            if(reply->type != REDIS_REPLY_ARRAY) {
                DARWIN_LOG_DEBUG("RedisManager::GetContextRole:: did not get expected answer while querying role");
                freeReplyObject(reply);
                return nullptr;
            }

            return reply;
        }


        bool RedisManager::SendPing(redisContext *context) {
            DARWIN_LOGGER;
            redisReply *reply = nullptr;
            bool success = true;

            if(not context) {
                DARWIN_LOG_CRITICAL("RedisManager::SendPing:: context is null, cannot ping");
                return false;
            }
            std::vector<const char *> arguments {"PING"};
            SendArgs(context, &reply, &arguments[0], arguments.size());

            if(reply and reply->type == REDIS_REPLY_ERROR) {
                DARWIN_LOG_WARNING("RedisManager::SendPing:: could not send ping -> "
                                + std::string(reply->str, reply->len));
                success = false;
            }
            else if(not reply){
                DARWIN_LOG_WARNING("RedisManager::SendPing:: could not send ping");
                success = false;
            }

            // Can accept nullptr
            freeReplyObject(reply);
            return success;
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


        redisContext* RedisManager::ConnectTo(const RedisConnectionInfo& connection, const time_t timeoutSec) {
            redisContext *context = nullptr;
            if (not connection.socketPath.empty())
                ConnectUnixSocket(connection.socketPath, &context, timeoutSec);
            else if(not connection.ip.empty())
                ConnectAddress(connection.ip, connection.port, &context, timeoutSec);
            return context;
        }


        bool RedisManager::ConnectUnixSocket(const std::string& unixSocket, redisContext **context, const time_t timeoutSec) {
            DARWIN_LOGGER;
            redisContext *tempContext;
            struct timeval timeout{timeoutSec, 0};

            if(unixSocket.empty()) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: unix socket path is empty, cannot connect");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: connecting to redis socket '" + unixSocket + "'...");

            if(timeoutSec > 0)
                tempContext = redisConnectUnixWithTimeout(unixSocket.c_str(), timeout);
            else
                tempContext = redisConnectUnix(unixSocket.c_str());

            if(not tempContext || tempContext->err) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: could not connect to unix socket '" + unixSocket + "'");
                redisFree(tempContext);
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectUnixSocket:: connected");
            *context = tempContext;
            return true;
        }

        bool RedisManager::ConnectAddress(const std::string& ip, const int port, redisContext **context, const time_t timeoutSec) {
            DARWIN_LOGGER;
            redisContext *tempContext;
            struct timeval timeout{timeoutSec, 0};

            if(ip.empty() || port == 0) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: ip and/or port incorrect, cannot connect");
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: connecting to Redis address '" + ip + ":" + std::to_string(port) + "'...");

            if(timeoutSec > 0)
                tempContext = redisConnectWithTimeout(ip.c_str(), port, timeout);
            else
                tempContext = redisConnect(ip.c_str(), port);

            if(not tempContext || tempContext->err) {
                DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: could not connect to server '" + ip + ":" + std::to_string(port) + "'");
                redisFree(tempContext);
                return false;
            }

            DARWIN_LOG_DEBUG("RedisManager::ConnectAddress:: connected");
            *context = tempContext;
            return true;
        }

        void RedisManager::AddReplicas(redisReply *replicasList, std::unordered_set<RedisConnectionInfo, RedisConnectionInfo>& set) {
            DARWIN_LOGGER;
            DARWIN_LOG_DEBUG("RedisManager::AddReplicas:: adding replicas...");
            // for each element in the list of replicas
            for(size_t i = 0; i < replicasList->elements; ++i) {
                // if the element in the list has 3  variables (ip, port, replication offset)
                if (replicasList->element[i]->elements == 3) {
                    //take and add it to the list of potential connections
                    RedisConnectionInfo replicaConn {
                        .socketPath = "",
                        .ip = replicasList->element[i]->element[0]->str,
                        // yep... that's a string...
                        .port = (unsigned int)std::stoi(replicasList->element[i]->element[1]->str)
                    };
                    DARWIN_LOG_DEBUG("RedisManager::AddReplicas:: added " + to_string(replicaConn));
                    set.emplace(replicaConn);
                }
            }
            DARWIN_LOG_DEBUG("RedisManager::AddReplicas:: done");
        }


        bool RedisManager::Discover() {
            DARWIN_LOGGER;
            redisContext *context = nullptr;
            redisReply *reply = nullptr;
            RedisConnectionInfo connection;
            bool connectionFound = false; //whether a valid connection/master was found

            std::unordered_set<RedisConnectionInfo, RedisConnectionInfo> newSet;
            std::unordered_set<RedisConnectionInfo, RedisConnectionInfo> searchList = this->_fallbackList;
            // add active and base connections to list if set
            if(this->_activeConnection.isSet())
                searchList.emplace(this->_activeConnection);
            if(this->_baseConnection.isSet())
                searchList.emplace(this->_baseConnection);

            DARWIN_LOG_INFO("RedisManager:: Discover:: beginning discovery...");

            auto it  = searchList.begin();
            auto end  = searchList.end();
            while(not connectionFound and it != end) {
                redisFree(context);
                freeReplyObject(reply);
                context = nullptr;
                reply = nullptr;
                DARWIN_LOG_DEBUG("RedisManager::Discover:: trying " + to_string(*it));
                context = ConnectTo(*it, this->_connectTimeout);
                if(context) {
                    reply = GetContextRole(context);
                    if(reply) {
                        if(strncmp(reply->element[0]->str, "master", 6) == 0) {
                            DARWIN_LOG_DEBUG("RedisManager::Discover:: found master " + to_string(*it));
                            connection = *it;
                            connectionFound = true;
                        } else if(strncmp(reply->element[0]->str, "slave", 5) == 0) {
                        DARWIN_LOG_DEBUG("RedisManager::Discover:: found replica " + to_string(*it));
                            DARWIN_LOG_DEBUG("RedisManager::Discover:: searching for master");
                            // Get master connection info
                            connection = RedisConnectionInfo({
                                .socketPath= "",
                                .ip = reply->element[1]->str,
                                .port = (unsigned int)reply->element[2]->integer
                            });
                            redisFree(context);
                            freeReplyObject(reply);
                            context = nullptr;
                            reply = nullptr;
                            context = ConnectTo(connection, this->_connectTimeout);
                            if(context) {
                                reply = GetContextRole(context);
                                if(reply && strncmp(reply->element[0]->str, "master", 6) == 0) {
                                    DARWIN_LOG_DEBUG("RedisManager::Discover:: found master " + to_string(connection));
                                    connectionFound = true;
                                }
                            }
                        }
                    }
                }
                ++it;
            }

            if(connectionFound) {
                // connection should hold valid connection infos
                std::unordered_set<RedisConnectionInfo, RedisConnectionInfo> newFallbacks;
                // check if reply holds a valid master reply, with list of replicas
                // add those replicas if so
                if(reply->elements == 3
                    && reply->element[2]->type == REDIS_REPLY_ARRAY
                    && reply->element[2]->elements > 0) {
                    AddReplicas(reply->element[2], newFallbacks);

                    // assign list of replicas to acquired list
                    std::swap(this->_fallbackList, newFallbacks);
                    DARWIN_LOG_INFO("RedisManager::Discover:: new fallbacks: " + to_string(this->_fallbackList));
                }
                //assign new master to valid connection (is master only if searchMaster was true)
                std::swap(this->_activeConnection, connection);
                DARWIN_LOG_INFO("RedisManager::Discover:: new active connection: " + to_string(this->_activeConnection));
            }
            else {
                DARWIN_LOG_INFO("RedisManager::Discover:: no valid connection found");
            }

            redisFree(context);
            freeReplyObject(reply);

            return connectionFound;
        }


        bool RedisManager::HealthCheck(std::shared_ptr<ThreadData> threadData) {
            DARWIN_LOGGER;

            // if the time between 2 calls is UNDER the time for a health check, assume connection is OK
            if(std::time(nullptr) < threadData->_redisLastUse + this->_healthCheckInterval)
                return true;

            // Cannot perform health check on absent active connection
            if(not threadData->_redisContext)
                return false;

            DARWIN_LOG_DEBUG("RedisManager::HealthCheck:: checking health of current connection");

            if(SendPing(threadData->_redisContext)) {
                std::time(&(threadData->_redisLastUse));
                return true;
            }

            DARWIN_LOG_INFO("RedisManager::HealthCheck:: could not ping, trying to reconnect...");
            if(this->ReconnectContext(threadData->_redisContext)) {
                std::time(&(threadData->_redisLastUse));
                DARWIN_LOG_INFO("RedisManager::HealthCheck:: reconnection successful");
                return true;
            }

            redisFree(threadData->_redisContext);
            threadData->_redisContext = nullptr;

            DARWIN_LOG_ERROR("RedisManager::HealthCheck:: could not reconnect");
            return false;
        }


        bool RedisManager::ReconnectContext(redisContext *context) {
            DARWIN_LOGGER;
            int status;
            RedisConnectionInfo connection;

            if(not context) {
                DARWIN_LOG_ERROR("RedisManager::ReconnectContext:: no valid connection, aborting");
                return false;
            }

            status = redisReconnect(context);

            if(status != REDIS_OK) {
                DARWIN_LOG_WARNING("RedisManager::ReconnectContext:: unable to reconnect using previous connection"
                                    ", trying to open a new one...");
                {
                    std::lock_guard<std::mutex> lock(this->_availableConnectionsMut);
                    connection = this->_activeConnection;
                }

                redisContext *newContext = ConnectTo(connection, this->_connectTimeout);
                if(newContext) {
                    redisFree(context);
                    context = newContext;
                    status = REDIS_OK;
                }
            }

            return status == REDIS_OK;
        }


        void RedisManager::DisconnectContext(redisContext **context) {
            redisFree(*context);
            *context = nullptr;
        }


        void RedisManager::ParseReply(redisReply *reply, std::any& reply_object) {
            if(not reply) return;
            DARWIN_LOGGER;

            switch(reply->type) {
                case REDIS_REPLY_ERROR:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseReply:: [string]: " + std::string(reply->str, reply->len));
                    return;
                case REDIS_REPLY_NIL:
                    return;
                case REDIS_REPLY_STATUS:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseReply:: [string]: " + std::string(reply->str, reply->len));
                    return;
                case REDIS_REPLY_INTEGER:
                    reply_object = reply->integer;
                    DARWIN_LOG_DEBUG("RedisManager::ParseReply:: [integer]: " + std::to_string(reply->integer));
                    return;
                case REDIS_REPLY_STRING:
                    reply_object = std::string(reply->str, reply->len);
                    DARWIN_LOG_DEBUG("RedisManager::ParseReply:: [string]: " + std::string(reply->str, reply->len));
                    return;
            }

            DARWIN_LOG_DEBUG("RedisManager::ParseReply:: [[array]]");
            std::vector<std::any> sub_array;
            for(size_t i = 0; i < reply->elements; i++) {
                std::any sub_object;
                ParseReply(reply->element[i], sub_object);
                sub_array.push_back(sub_object);
            }
            if(not sub_array.empty()) reply_object = sub_array;
            return;
        }
    }
}
