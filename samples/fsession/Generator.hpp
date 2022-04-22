/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     30/08/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

extern "C" {
#include <hiredis/hiredis.h>
}

#include <mutex>
#include <thread>
#include <string>

#include "ATask.hpp"
#include "../../toolkit/RedisManager.hpp"
#include "../toolkit/rapidjson/document.h"
#include "AGenerator.hpp"

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator() = default;

    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const;

private:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;
};