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

#include "Session.hpp"
#include "../toolkit/rapidjson/document.h"

class AGenerator {
public:
    AGenerator() = default;
    virtual ~AGenerator() = default;

// Methods to be implemented by the child
public:
    /// Create a new task.
    ///
    /// \param socket The Session's socket.
    /// \param manager The Session manager.
    /// \return A pointer to a new session.
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept = 0;

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
    virtual bool ConfigureAlterting(const std::string& tags) = 0;

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

protected:
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache; //!< The cache for already processed request
    std::mutex _cache_mutex;
};
