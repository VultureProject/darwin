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
#include "AGenerator.hpp"
#include "HostLookupTask.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "../toolkit/Files.hpp"
#include "../toolkit/rapidjson/document.h"

class Generator: public AGenerator {
public:
    Generator(size_t nb_task_threads);
    ~Generator() = default;

    virtual std::shared_ptr<darwin::ATask>
    CreateTask(darwin::session_ptr_t s) noexcept override final;

    virtual long GetFilterCode() const;

protected:
    enum class db_type {
        text = 0,
        json,
        rsyslog
    };

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    virtual bool ConfigureAlerting(const std::string& tags) override final;

    bool LoadTextFile(const std::string& filename);
    bool LoadJsonFile(const std::string& filename, const db_type type);
    void LoadFeedNameFromFile(const std::string& filename);
    bool LoadJsonDatabase(const rapidjson::Document& database);
    bool LoadJsonEntry(const rapidjson::Value& entry);
    bool LoadRsyslogDatabase(const rapidjson::Document& database);
    bool LoadRsyslogEntry(const rapidjson::Value& entry);

private:
    // This implementation is thread safe with multiple reader
    // with no writer.
    // This is indicated by the repository doc.
    // It should mimic thread safety of std::unordered_map<>
    tsl::hopscotch_map<std::string, std::pair<std::string, int>> _database; //!< The "bad" hostname database
    std::string _feed_name;
};