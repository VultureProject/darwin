/// \file     AGenerator.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     16/10/2019
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <fstream>
#include <string>
#include <thread_pool.hpp>

#include "ATask.hpp"
#include "../toolkit/rapidjson/document.h"

class AGenerator {
public:
    AGenerator(size_t nb_task_threads);
    virtual ~AGenerator() = default;


// Methods to be implemented by the child
public:
    ///
    /// \brief Create a Task object
    /// 
    /// \param s a shred pointer to the sessions creating the task
    /// \return std::shared_ptr<darwin::ATask> a shared pointer to the created task
    ///
    virtual std::shared_ptr<darwin::ATask> CreateTask(darwin::session_ptr_t s) noexcept = 0;
    
    virtual bool ConfigureNetworkObject(boost::asio::io_context &context);

protected:
    /// Configure the child object using the read configuration file.
    /// The file is already proven to be a JSON format.
    /// This methods only get the requested fields from the json document.
    ///
    /// \param configuration The rapidjson document ready to be used.
    /// \return True on success, false on failure.
    virtual bool LoadConfig(const rapidjson::Document &configuration) = 0;

    /// Configure the Alerting with the informations provided by the filter.
    /// It is used to configure the alert's filter_name, rule_name and tags.
    ///
    /// \param tags The custom tags set by the user. If empty, set default tags.
    /// \return true if everything went right, false otherwise.
    virtual bool ConfigureAlerting(const std::string& tags) = 0;

// Final methods and abstract attributes
public:
    /// Configure the generator from file and create cache.
    ///
    /// \param configurationFile Path to the configuration path.
    /// \param cache_size Size to allocate for the lru cache.
    /// \return True on success, false on failure.
    virtual bool
    Configure(std::string const& configFile,
              const std::size_t cache_size) final;

    virtual tp::ThreadPool& GetTaskThreadPool() final;

private:
    /// Open and read the configuration file.
    /// Try to load the json format of the configuration.
    ///
    /// \param configuration_file_path Path to the configuration path.
    /// \return True on success, false on failure.
    virtual bool ReadConfig(const std::string &configuration_file_path) final;

    /// Extract the "alert_tags" optional string list from the conf and
    /// store it in the `tags` parameter.
    ///
    /// \param configuration The rapidjson object containing the parsed configuration
    /// \param tags The string to receive the configured tags
    /// \return true if custom tags were found, false otherwise
    virtual bool ExtractCustomAlertingTags(const rapidjson::Document &configuration,
                                           std::string& tags);

    ///
    /// \brief Get the configuration object of the task thread pool
    /// 
    /// \param nb_task_threads number of workers (threads) to spawn for the tasks
    /// \return tp::ThreadPoolOptions 
    ///
    static tp::ThreadPoolOptions GetThreadPoolOptions(size_t nb_task_threads);

protected:
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache; //!< The cache for already processed request
    std::mutex _cache_mutex;

    tp::ThreadPool _threadPool;

};
