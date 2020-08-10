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
#include "AGenerator.hpp"
#include "AlertManager.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "Yara.hpp"

class Generator : public AGenerator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string &tags) override final;

private:
    bool _fastmode;
    int _timeout;
    std::shared_ptr<darwin::toolkit::YaraCompiler> _yaraCompiler = nullptr;
    // The cache for already processed request
    std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> _cache;
};