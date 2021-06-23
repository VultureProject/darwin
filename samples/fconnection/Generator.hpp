/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "../toolkit/rapidjson/document.h"
#include "../../toolkit/RedisManager.hpp"
#include "ATask.hpp"
#include "AGenerator.hpp"

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator();

public:
    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const override final;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

private:
    bool ConfigRedis(const std::string &redis_socket_path,
                     const std::string &init_data_path);

    unsigned int _redis_expire = 0;
};