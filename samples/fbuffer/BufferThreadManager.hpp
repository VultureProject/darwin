/// \file     BufferThreadManager.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <any>
#include <string>
#include <thread>

#include "../../toolkit/RedisManager.hpp"
#include "Connectors.hpp"

class BufferThread {
    public:
    BufferThread(std::shared_ptr<AConnector> output);
    ~BufferThread() = default;

    public:
    bool Stop();

    private:
    /// The loop in which the thread main will be executed every _interval seconds
    void ThreadMain();
    /// Main function
    bool Main();

    /// Write the alerts/logs in Redis
    /// \return true on success, false otherwise.
    //TODO legacy function bound to disappear
    bool WriteRedis(const std::string& log_line);

    /// Get the logs from the Redis
    /// \param rangeLen the range of logs we want from the redis list
    /// \param logs the vector where we want to stock our logs
    /// \return true on success, false otherwise.
    bool REDISPopLogs(long long int len, std::vector<std::string> &logs) noexcept;

    /// Remove the already processed logs in Redis
    /// \param rangeLen the range of logs we want to tirm in redis
    /// \return true on success, false otherwise.
    bool REDISReinsertLogs(std::vector<std::string> &logs) noexcept;

    /// Query the Redis to get length of the list
    /// where we have our data
    /// \return true on success, false otherwise.
    long long int REDISListLen() noexcept;


    private:
    static constexpr int MIN_LOGS_LINES = 10; // TODO : A déplacer dans le fichier de config du fbuffer, pour chacun des outputs

    int _interval; // Interval between 2 wake-ups in seconds.
    std::thread _thread;
    std::atomic<bool> _is_stop{true}; // To know if the thread is stopped or not
    std::condition_variable cv;
    std::mutex _mutex; // The mutex used to manage multiple acces to the _thread member
    std::shared_ptr<AConnector> _connector;
    std::string _redis_list;
};

class BufferThreadManager {
public:
    BufferThreadManager(int max_nb_thread, std::string &redis_internal);
    ~BufferThreadManager();

public:
    /// Start the thread
    ///
    /// \return true in success, else false
    /// \param interval the number of seconds between 2 wake-ups (in seconds), defaults to 300
    bool Start(std::shared_ptr<AConnector> output);
    /// Stop the thread
    ///
    /// \return true in success, else false
    bool Stop();

private:
    unsigned long _max_nb_threads; // TODO : Change for _nb_threads
    std::vector<std::shared_ptr<BufferThread>> _threads;


private:
    unsigned int _interval; // Interval in which the thread main function will be executed (in seconds)

private:
    const std::string _redis_internal; // redis' list which contain our data
    bool _is_log_redis; //is there a channel and/or list to put alerts to ?
    std::shared_ptr<std::mutex> _redis_mutex;
};
