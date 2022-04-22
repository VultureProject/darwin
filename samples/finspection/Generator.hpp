/// \file     Generator.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include "ATask.hpp"
#include "data_pool.hpp"
#include "AGenerator.hpp"
#include "ContentInspectionTask.hpp"


class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator();

public:
    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

private:
    Configurations _configurations;
    MemManagerParams *_memoryManager;
};