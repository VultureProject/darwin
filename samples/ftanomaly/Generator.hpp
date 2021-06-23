/// \file     Generator.hpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "../toolkit/FileManager.hpp"
#include "ATask.hpp"
#include "TAnomalyThreadManager.hpp"
#include "AGenerator.hpp"

#define REDIS_INTERNAL_LIST "_anomalyFilter_internal"

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator() = default;

    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const override final;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

    std::string _redis_internal = REDIS_INTERNAL_LIST;
    std::shared_ptr<AnomalyThreadManager> _anomaly_thread_manager;
};