#ifndef DARWIN_REDISMANAGER_HPP
#define DARWIN_REDISMANAGER_HPP

extern "C" {
#include <hiredis/hiredis.h>
}

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <set>
#include <any>

namespace darwin {

    namespace toolkit {

        class ThreadData {
        public:
            ThreadData(){};
        public:
            std::thread::id thread_id;
            redisContext* _masterContext = nullptr;
            std::mutex _masterContextMut;
            time_t _masterLastUse;
            redisContext* _slaveContext = nullptr;
            std::mutex _slaveContextMut;
            time_t _slaveLastUse;
        };

        class RedisManager {

        public:
            static constexpr int JANITOR_RUN_INTERVAL = 5;
            static constexpr int CONNECTION_DROP_TIMEOUT = 20;

        public:
            static RedisManager& GetInstance();

            ~RedisManager();

            RedisManager(RedisManager const&) = delete;
            void operator=(RedisManager const&) = delete;

            /// Set Unix socket path
            ///
            /// \param fullpath the full system path to the socket
            /// \return whether the manager could connect to the socket
            bool SetUnixPath(const std::string &fullpath);

            /// Set IP:port
            ///
            /// \param ip the ip of the Redis server (without the port)
            /// \param port the port to connect to
            /// \return whether the manager could connect to the server
            bool SetIpAddress(const std::string &ip, const int port);

            /// Execute a query in Redis.
            ///
            /// Do not wait for any value
            ///
            /// \param arguments the arguments to send to redis
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL, REDIS_REPLY_STRING, REDIS_REPLY_ARRAY]
            int Query(const std::vector<std::string> &arguments);

            /// Execute a query in Redis.
            ///
            /// Expect an integer returned in reply_int
            /// \warning value might not be returned, check response code before using data
            ///
            /// \param arguments the arguments to send to redis
            /// \param reply_int a reference to an int to store the answer number sent by Redis (if applicable).
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL, REDIS_REPLY_STRING, REDIS_REPLY_ARRAY]
            int Query(const std::vector<std::string> &arguments, long long int& reply_int);

            /// Execute a query in Redis.
            ///
            /// Expect a string returned in reply_string
            /// \warning value might not be returned, check response code before using data
            ///
            /// \param arguments the arguments to send to redis
            /// \param reply_string a reference to an int to store the answer number sent by Redis (if applicable).
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL, REDIS_REPLY_STRING, REDIS_REPLY_ARRAY]
            int Query(const std::vector<std::string> &arguments, std::string& reply_string);

            /// Execute a query in Redis.
            ///
            /// Expect array of data returned in reply_array
            /// \warning value might not be returned, check response code before using data
            ///
            /// \param arguments the arguments to send to redis
            /// \param reply_array a reference to an array of std::any to store the answers sent by Redis (if applicable).
            /// \return The reply type in [REDIS_REPLY_STATUS, REDIS_REPLY_ERROR, REDIS_REPLY_INTEGER, REDIS_REPLY_NIL, REDIS_REPLY_STRING, REDIS_REPLY_ARRAY]
            int Query(const std::vector<std::string> &arguments, std::any& reply_array);

        private:
            RedisManager();

            void JanitorRun();

            /// \warning associated mutex should already be locked
            bool SendPing(redisContext *context);

            /// \warning associated mutex should already be locked
            bool SendArgs( redisContext *context,
                            redisReply **reply_ptr,
                            const char **formatted_arguments,
                            int arguments_number);

            std::shared_ptr<ThreadData> GetThreadInfo();

            bool ConnectUnixSocket(const std::string unixSocket, redisContext **context);

            bool ConnectAddress(const std::string masterIp, const int masterPort, redisContext **context);

            std::string GetRole(redisContext *context, std::mutex& contextMutex, std::string& masterIp, int& masterPort);

            bool Discover();

            /// \warning associated mutex should already be locked
            bool Reconnect(redisContext *context);

            void ParseReply(redisReply *reply, std::any& reply_object);

        private:
            std::thread _janitor; // thread to keep connections active, and close inactive ones
            bool _janitorStop; // variable to notify janitor to stop
            int _janitorRunInterval; // time between 2 janitor runs
            std::mutex _janitorMut; // mutex for the janitor condition variable
            std::condition_variable _janitorCondVar; // condition variable for janitor
            std::set<std::shared_ptr<ThreadData>> _threadSet; // set containing each thread own context
            std::mutex _setMut; // set mutex
            std::string _masterUnixSocket;
            std::string _masterIp;
            int _masterPort;
            std::string _slaveUnixSocket;
            std::string _slaveIp;
            int _slavePort;
        };
    }
}

#endif //DARWIN_REDISMANAGER_HPP
