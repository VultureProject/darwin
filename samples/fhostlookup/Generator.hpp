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

#include "Session.hpp"
#include "AGenerator.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "../toolkit/Files.hpp"
#include "../toolkit/rapidjson/document.h"

class Generator: public AGenerator {
public:
    Generator() = default;
    ~Generator() = default;

public:
    virtual darwin::session_ptr_t
    CreateTask(boost::asio::local::stream_protocol::socket& socket,
               darwin::Manager& manager) noexcept override final;

protected:
    virtual bool LoadConfig(const rapidjson::Document &configuration) override final;
    bool LoadDatabase(const rapidjson::Document& database);
    bool LoadEntry(const rapidjson::Value& entry);

private:
    // This implementation is thread safe with multiple reader
    // with no writer.
    // This is indicated by the repository doc.
    // It should mimic thread safety of std::unordered_map<>
    tsl::hopscotch_map<std::string, int> _database; //!< The "bad" hostname database
    std::string _feed_name;
};