/// \file     Generator.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     10/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "Session.hpp"
#include "../toolkit/rapidjson/document.h"
#include "../toolkit/Yara.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    // The config file is the database here
    bool Configure(std::string const& configFile, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);

private:
    bool _fastmode;
    int _timeout;
    std::shared_ptr<darwin::toolkit::YaraCompiler> _yaraCompiler = nullptr;
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};