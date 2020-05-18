/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     07/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "HostLookupTask.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"
#include "../toolkit/rapidjson/document.h"
#include "../toolkit/rapidjson/istreamwrapper.h"


bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Loading classifier...");
    std::string  db;
    rapidjson::Document database;

    if (!configuration.HasMember("database")) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Missing parameter: 'database'");
        return false;
    }
    if (!configuration["database"].IsString()) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: 'database' needs to be a string");
        return false;
    }
    db = configuration["database"].GetString();
    std::ifstream file(db.c_str());
    if (!file) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
        return false;
    }

    DARWIN_LOG_DEBUG("HostlookupGenerator:: Parsing database...");
    rapidjson::IStreamWrapper isw(file);
    database.ParseStream(isw);
    if (not this->LoadDatabase(database)) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool Generator::LoadDatabase(const rapidjson::Document& database) {
    DARWIN_LOGGER;
    if (not database.IsObject()) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: Database is not a JSON object");
        return false;
    }
    if (not database.HasMember("feed_name") or not database["feed_name"].IsString()) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: No proper feed name provided in the database");
        return false;
    }
    this->_feed_name = database["feed_name"].GetString();
    if (not database.HasMember("data") or not database["data"].IsArray()) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: No or ill formated entries in the database");
        return false;
    }
    auto entries = database["data"].GetArray();
    if (entries.Size() == 0) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: No entry in the database. Stopping.");
        return false;
    }
    for (auto& entry : entries) {
        this->LoadEntry(entry);
    }
    if (this->_database.size() == 0) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: No usable entry in the database. Stopping.");
        return false;
    }
    return true;
}

bool Generator::LoadEntry(const rapidjson::Value& entry) {
    DARWIN_LOGGER;
    int score = 100;
    std::string sentry;
    if (not entry.IsObject()) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: Database entry is not a JSON object. Ignoring.");
        return false;
    }
    if (not entry.HasMember("entry") or not entry["entry"].IsString()) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: Entry is not a string. Ignoring.");
        return false;
    }
    if (entry.HasMember("score") and entry["score"].IsInt()) {
        score = entry["score"].GetInt();
        if (score < 0 or score > 100) {
            DARWIN_LOG_WARNING("HostlookupGenerator:: Found score not between 0 and 100 in database " + this->_feed_name + ", setting to 100");
            score = 100;
        }
    }
    this->_database[entry["entry"].GetString()] = score;
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<HostLookupTask>(socket, manager, _cache, _cache_mutex, _database, _feed_name));
}
