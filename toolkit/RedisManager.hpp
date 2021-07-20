#ifndef DARWIN_REDISMANAGER_HPP
#define DARWIN_REDISMANAGER_HPP

extern "C" {
#include <hiredis/hiredis.h>
}

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <unordered_set>
#include <set>
#include <any>
#include <ostream>
#include <sstream>

#include "xxhash.h"
#include "xxhash.hpp"

// defined by hiredis:
// REDIS_REPLY_STRING 1
// REDIS_REPLY_ARRAY 2
// REDIS_REPLY_INTEGER 3
// REDIS_REPLY_NIL 4
// REDIS_REPLY_STATUS 5
// REDIS_REPLY_ERROR 6
static constexpr int REDIS_CONNNECTION_ERROR = 7;
// WARNING requires to be careful of changes in hiredis library, but allows optimisations in code


namespace darwin {

    namespace toolkit {

        class ThreadData {
        public:
            ThreadData(){};
        public:
            redisContext* _redisContext = nullptr;
            time_t _redisLastUse = 0;
            time_t _lastDiscovery = 0;
            size_t _retryAttempts = 0;
        };

        struct RedisConnectionInfo {
            std::string socketPath;
            std::string ip;
            unsigned int port;

            size_t operator()(const RedisConnectionInfo& obj) const {
                return (size_t)xxh::xxhash<32>(obj.socketPath + obj.ip + std::to_string(obj.port));
            }

            bool operator==(const RedisConnectionInfo& obj) const {
                return socketPath == obj.socketPath
                        and ip == obj.ip
                        and port == obj.port;
            }
            
            RedisConnectionInfo() = default;

            RedisConnectionInfo(const std::string &socketPath, const std::string &ip, unsigned int port) : 
                socketPath(socketPath), 
                ip(ip), 
                port(port) {}

            RedisConnectionInfo(const RedisConnectionInfo &obj) {
                socketPath.assign(obj.socketPath);
                ip.assign(obj.ip);
                port = obj.port;
            }

            void operator=(const RedisConnectionInfo& obj) {
                socketPath.assign(obj.socketPath);
                ip.assign(obj.ip);
                port = obj.port;
            }

            bool isSet() {
                return not socketPath.empty() or (not ip.empty() and port != 0);
            }
        };

        inline std::string to_string(const RedisConnectionInfo& connectionInfo)
        {
            std::ostringstream ss;

            if(not connectionInfo.socketPath.empty()) {
                ss << connectionInfo.socketPath;
            }
            else {
                ss << connectionInfo.ip << ":" << connectionInfo.port;
            }
            return std::move(ss).str();
        }

        inline std::string to_string(const std::unordered_set<RedisConnectionInfo, RedisConnectionInfo>& connectionInfoList)
        {
            std::ostringstream ss;
            ss << "[ ";
            for(auto it = connectionInfoList.begin(); it != connectionInfoList.end(); ++it) {
                ss << to_string(*it) << " ";
            }
            ss << "]";
            return std::move(ss).str();
        }


        class RedisManager {

        public:
            // default values
            static constexpr int HEALTH_CHECK_INTERVAL = 8;  // more than 8 seconds without calling redis will trigger a health check
            static constexpr int CONNECTION_TIMEOUT = 5;  // 5 seconds for connections' timeout
            static constexpr size_t MAX_RETRY_ATTEMPTS = 2;

        public:
            RedisManager(const RedisManager&) = delete;
            void operator=(const RedisManager&) = delete;

            RedisManager() = default;
            ~RedisManager();

            static RedisManager& GetInstance();

            /// Sets the value to take into account for helth check intervals
            /// will be used to trigger health checks AND reconnections attempts during queries (if asked)
            /// \param interval the seconds between 2 checks/reconnections attempts
            void SetHealthCheckInterval(const unsigned int interval);

            /// Sets the timeout used during connections
            void SetTimeoutConnect(const unsigned int timeoutSec);

            /// Sets the _baseConnection internal attribute to use for initial connection attempts and discoveries
            /// \param fullpath the full system path to the unix socket
            void SetUnixConnection(const std::string& fullpath);

            /// Sets the _baseConnection internal attribute to use for initial connection attempts and discoveries
            /// \param ip the ip to connect to
            /// \param port the port to use during connection attempt
            void SetIpConnection(const std::string& ip, const unsigned int port);

            /// Static function to test if the server accessed is a master
            /// Won't change any attribute in the RedisManager, but can be useful to check for servers' statuses
            /// \param unisSocket the full system path to the unix socket
            static bool TestIsMaster(const std::string& unixSocket);

            /// Static function to test if the server accessed is a master
            /// Won't change any attribute in the RedisManager, but can be useful to check for servers' statuses
            /// \param ip the ip to connect to
            /// \param port the port to use during connection
            static bool TestIsMaster(const std::string& ip, const unsigned int port);

            /// Tries to connect thread using internal class' connection attributes
            /// Will use _activeConnection if available, _baseConnection otherwise
            /// \warning add connection mediums through SetUnixConnection and/or SetIpConnection before calling Connect
            /// \warning affects thread data
            /// \warning will update the thread's connection object upon successful connection
            /// \return true if the manager could connect to a redis server, false otherwise
            bool Connect();

            /// Disconnects the calling thread from its connected Redis instance
            /// \warning affects thread data
            void Disconnect();

            /// Get the connected server's role
            /// \warning affects thread data
            /// \warning might invalidate thread's connection
            /// \return true if the server is a master, false in every other situation
            bool IsMaster();

            /// Tries to use known connections to query valid redis master server's address
            /// and assign thread's redis context to it
            /// \warning affects thread AND instance data
            /// \warning might invalidate thread's connection
            /// \warning may change list of known servers and defined main connection
            /// \return true if a valid redis master was found, false otherwise
            bool FindAndConnect();

            /// Tries to connect to currently defined main connection
            /// If current main connection is invalid, tries to find a new one with \see FindAndConnect()
            /// Prefered way to handle disconnection cases during filters normal processing,
            ///     to prevent flooding network with reconnections
            /// \warning affects thread AND instance data
            /// \warning might not try anything if calls are too frequent (defined by health check interval)
            /// \return true if a valid master connection was found, false otherwise
            bool FindAndConnectWithRateLimiting();

            /// Returns the role of the currently connected redis server
            /// \warning affects thread data
            /// \warning will invalidate thread's redis connection if an error is encountered during call
            /// \return either 'master', 'replica', 'sentinel', or an empty string in case of failure
            std::string GetRole();

            /// Execute a query in Redis
            /// Do not wait for any value
            /// \warning affects thread AND instance data
            /// \warning can invalidate thread's connection in case of error
            /// \warning if reconnection is triggered, will affect instance's known hosts and thread current connection
            /// \param arguments the arguments to send to redis
            /// \param reconnectRetry in case of connection/insertion error, tries to discover/reconnect to a valid
            ///         master and retry call
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL,
            ///                             REDIS_REPLY_STRING, REDIS_REPLY_ARRAY, REDIS_CONNNECTION_ERROR]
            int Query(const std::vector<std::string>& arguments, bool reconnectRetry = false);

            /// Execute a query in Redis.
            /// Expect an integer returned in reply_int
            /// \warning affects thread AND instance data
            /// \warning can invalidate thread's connection in case of error
            /// \warning if reconnection is triggered, will affect instance's known hosts and thread current connection
            /// \warning value might not be returned, check response code before using data
            /// \param arguments the arguments to send to redis
            /// \param reply_int a reference to an int to store the answer number sent by Redis (if applicable).
            /// \param reconnectRetry in case of connection/insertion error, tries to discover/reconnect to a valid
            ///         master and retry call
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL,
            ///                             REDIS_REPLY_STRING, REDIS_REPLY_ARRAY, REDIS_CONNNECTION_ERROR]
            int Query(const std::vector<std::string>& arguments, long long int& reply_int, bool reconnectRetry = false);

            /// Execute a query in Redis.
            /// Expect a string returned in reply_string
            /// \warning affects thread AND instance data
            /// \warning can invalidate thread's connection in case of error
            /// \warning if reconnection is triggered, will affect instance's known hosts and thread current connection
            /// \warning value might not be returned, check response code before using data
            /// \param arguments the arguments to send to redis
            /// \param reply_string a reference to an int to store the answer number sent by Redis (if applicable).
            /// \param reconnectRetry in case of connection/insertion error, tries to discover/reconnect to a valid
            ///         master and retry call
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL,
            ///                             REDIS_REPLY_STRING, REDIS_REPLY_ARRAY, REDIS_CONNNECTION_ERROR]
            int Query(const std::vector<std::string>& arguments, std::string& reply_string, bool reconnectRetry = false);

            /// Execute a query in Redis.
            /// Expect array of data returned in reply_array
            /// \warning affects thread AND instance data
            /// \warning can invalidate thread's connection in case of error
            /// \warning if reconnection is triggered, will affect instance's known hosts and thread current connection
            /// \warning value might not be returned, check response code before using data
            /// \param arguments the arguments to send to redis
            /// \param reply_array a reference to an array of std::any to store the answers sent by Redis (if applicable).
            /// \param reconnectRetry in case of connection/insertion error, tries to discover/reconnect to a valid
            ///         master and retry call
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL,
            ///                             REDIS_REPLY_STRING, REDIS_REPLY_ARRAY, REDIS_CONNNECTION_ERROR]
            int Query(const std::vector<std::string>& arguments, std::any& reply_array, bool reconnectRetry = false);

        private:
            /// Internal function to query for thread related data (active connection, etc...)
            std::shared_ptr<ThreadData> GetThreadInfo();

            /// Gets the role of the redis server
            /// \warning will invalidate thread's connection in case of connection error
            /// \param context the pointer to a valid context (or a nullptr)
            /// \return a pointer to the redisReply object received from the server, or nullptr
            static redisReply* GetContextRole(redisContext *context);

            /// Sends a ping to the redis server relative to the context given
            /// \param context the pointer to a valid context
            /// \return true if the server replied, false otherwise
            static bool SendPing(redisContext *context);

            /// Wrapper to command to a Redis server and get the answer
            /// \param context the pointer to a valid context
            /// \param reply_ptr a double pointer on a redisReply, will host the reply given by the server
            /// \param formatted_arguments the command with arguments to give to the server
            /// \param arguments_number the number of arguments in formatted_arguments (including command)
            /// \return true if the call succeeded, false otherwise
            static bool SendArgs( redisContext *context,
                            redisReply **reply_ptr,
                            const char **formatted_arguments,
                            int arguments_number);

            /// Tries to connect to a Redis Server
            /// \param connection the object containing the server connection method
            /// \param timeoutSec the number of seconds before connection timeout (default is 0 = no timeout)
            /// \return a pointer on a valid redisContext, or nullptr
            static redisContext* ConnectTo(const RedisConnectionInfo& connection, const time_t timeoutSec = 0);

            /// Connect to a Redis server with an unix socket
            /// \param unixSocket the full system path to the socket
            /// \param context a pointer to a pointer to a redisContext, will contain the valid context if the
            /// connection was successfully initiated
            /// \param timeoutSec seconds before connection attempt timeout, 0 means function will wait indefinitely
            /// \return true if the connection was successfully initiated, false otherwise
            static bool ConnectUnixSocket(const std::string& unixSocket, redisContext **context, const time_t timeoutSec = 0);

            /// Connect to a Redis Server with an IP and port
            /// \param ip the ip to connect to
            /// \param port the port to use
            /// \param context a pointer to a pointer to a redisContext, will contain the valid context if the
            /// connection was successfully initiated
            /// \param timeoutSec seconds before connection attempt timeout, 0 means function will wait indefinitely
            /// \return true if the connection was successfully initiated, false otherwise
            static bool ConnectAddress(const std::string& ip, const int port, redisContext **context, const time_t timeoutSec = 0);

            /// Helper function to add all associated master's replicas to the set of available connections
            /// \param replicasList a redisReply* list representing what is returned from a master when its role is asked
            /// \param set a reference to the set of available connections of the RedisManager, will bet updated with the connections from replicasList
            static void AddReplicas(redisReply *replicasList, std::unordered_set<RedisConnectionInfo, RedisConnectionInfo>& set);

            /// Make use of known connections in instance to discover potential linked servers
            /// \warning affects instance's data
            /// \warning will modify main connection and replicas on success
            /// \return true if at least a valid Redis master was found, false otherwise
            bool Discover();

            /// Runs a connection health check if the health check interval exceeds the one set (default 8 seconds)
            /// \param threadData a valid shared pointer on a ThreadData instance
            /// \warning affect thread's data
            /// \warning might invalidate thread's current connection if server doesn't respond
            /// \return true if connection is sane (or health check was done recently), false otherwise
            bool HealthCheck(std::shared_ptr<ThreadData> threadData);

            /// Will try to reconnect a valid context
            /// if old connection doesn't work, will try to connect to current instance's main connection
            /// \param context a pointer to a valid context
            /// \warning might modify context
            /// \return true on successful reconnection, false otherwise
            bool ReconnectContext(redisContext *context);

            /// Disconnects the context given as param
            /// \param context a double pointer on the context to disconnect
            static void DisconnectContext(redisContext **context);

            /// A helper function to parse the redisReply object given by a Redis server
            /// \param reply a pointer to a valid redisReply to parse
            /// \param reply_object a reference to an any object to store the returned data
            static void ParseReply(redisReply *reply, std::any& reply_object);

        private:
            time_t _healthCheckInterval = HEALTH_CHECK_INTERVAL; // will execute a HealthCheck if the connection wasn't used for x seconds
            time_t _connectTimeout = 0; // timeout when trying to connect
            size_t _maxRetryAttempts = MAX_RETRY_ATTEMPTS;
            std::set<std::shared_ptr<ThreadData>> _threadSet; // set containing each thread's own context
            std::mutex _threadSetMut; // set mutex
            RedisConnectionInfo _activeConnection; // current active connection
            RedisConnectionInfo _baseConnection; // first connection provided (useful to keep unix sockets)
            std::unordered_set<RedisConnectionInfo, RedisConnectionInfo> _fallbackList; // all other possible connections
            std::mutex _availableConnectionsMut; // connections mutex
        };
    }
}

#endif //DARWIN_REDISMANAGER_HPP
