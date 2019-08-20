/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     16/01/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <maxminddb.h>
}

#include <map>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "Session.hpp"
#include "tensorflow/core/public/session.h"

class Generator {
public:
    Generator() = default;
    ~Generator();

public:
    static constexpr int DEFAULT_MAX_TOKENS = 50;
    // The config file is the database here
    bool Configure(std::string const &model_path, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    bool SetUpClassifier(const std::string &configuration_file_path);
    bool LoadClassifier(const rapidjson::Document &configuration);
    bool LoadTokenMap(const std::string &token_map_path);
    bool LoadModel(const std::string &model_path);

    std::shared_ptr<tensorflow::Session> _session;
    std::map<std::string, unsigned int> _token_map;
    unsigned int _max_tokens = 50;
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};
