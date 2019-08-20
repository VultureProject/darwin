/// \file     Generator.hpp
/// \authors  gcatto
/// \version  1.0
/// \date     06/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <set>
#include <xgboost/c_api.h>

#include "Session.hpp"

class Generator {
public:
    Generator() = default;
    ~Generator();
    void LoadKeywords();
    bool LoadClassifier(const std::string &model_path);

public:
    // The config file is the database here
    bool Configure(std::string const &model_path, const std::size_t cache_size);

    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept;

private:
    BoosterHandle _booster = nullptr; // The XGBoost classifier
    std::set<std::string> _keywords; // The keywords used to classify the requests
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};
