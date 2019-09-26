#ifndef DARWIN_REDISMANAGER_HPP
#define DARWIN_REDISMANAGER_HPP

extern "C" {
#include <hiredis/hiredis.h>
}

#include <thread>
#include <memory>
#include <mutex>
#include <vector>


/// \namespace darwin
namespace darwin {
    /// \namespace toolkit
    namespace toolkit {

        class RedisManager {

        public:
            static constexpr int PING_INTERVAL = 4;
            static constexpr int MAX_RETRIES = 2;
            static constexpr unsigned int MAX_QUERY_RETRIES = 2;

        public:
            explicit RedisManager(std::string socket_path);

            ~RedisManager();

            bool ConnectToRedis(bool keepAlive=false);

            /// Execute a query in Redis.
            ///
            /// \return true on success, false otherwise.
            bool REDISQuery(redisReply **reply_ptr,
                            const std::vector<std::string> &arguments) noexcept;

            bool ConnectToRedisMaster();
            bool ConnectWithIp(std::string ip, int port);

            /// To know if the connection is on master
            /// \param masterIp filled with master ip if we find it
            /// \param masterPort filled with master port if we find it
            /// \return -1 if failure,
            /// 0 if it's slave,
            /// 1 if it's master
            int IsMaster(std::string &masterIp, long long int &masterPort);


        private:
            void KeepConnectionAlive();
            bool REDISSendArgs(redisReply **reply_ptr,
                               const char **formatted_arguments,
                               int arguments_number);

        private:
            bool _is_stop;

            redisContext *_redis_connection = nullptr; // The redis handler

            const std::string _redis_socket_path; // Redis UNIX socket path
            std::mutex _redis_mutex; // hiredis is not thread safe
            std::thread _send_ping_requests; // necessary to keep connection alive from Redis

        };
    }
}

#endif //DARWIN_REDISMANAGER_HPP
