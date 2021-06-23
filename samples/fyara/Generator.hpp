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

#include "ATask.hpp"
#include "AGenerator.hpp"
#include "AlertManager.hpp"
#include "../../toolkit/rapidjson/document.h"
#include "Yara.hpp"

class Generator : public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator() = default;

public:
    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const override final;

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