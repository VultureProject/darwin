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

protected:
    /// Open and read the configuration file.
    /// Try to load the json format of the configuration.
    ///
    /// \param configuration_file_path Path to the configuration path.
    /// \return True on success, false on failure.
    virtual bool ReadConfig(const std::string &configuration_file_path) final;

protected:
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache; //!< The cache for already processed request
};