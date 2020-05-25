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
    std::string db;
    std::string db_type;

    // Load DB Name
    if (!configuration.HasMember("database")) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Missing parameter: 'database'");
        return false;
    }
    if (!configuration["database"].IsString()) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: 'database' needs to be a string");
        return false;
    }
    db = configuration["database"].GetString();

    // Load DB Type
    if (configuration.HasMember("db_type")) {
            if (!configuration["db_type"].IsString()) {
            DARWIN_LOG_CRITICAL("HostLookup:: Generator:: 'db_type' needs to be a string");
            return false;
        }
        db_type = configuration["db_type"].GetString();
    } else {
        db_type = "text";
    }

    // Load the DB according to the given type
    if (db_type == "json") {
        return this->LoadJsonFile(db);
    } else if (db_type == "text") {
        return this->LoadTextFile(db);
    } else {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Unknown 'db_type'");
        return false;
    }
    return false; // Put it there just in case.
}

bool Generator::LoadTextFile(const std::string& filename) {
    DARWIN_LOGGER;
    std::ifstream file(filename.c_str());
    std::string buf;

    // Load file content
    if (!file) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
        return false;
    }
    while (!darwin::files_utils::GetLineSafe(file, buf).eof()) {
        if(file.fail() or file.bad()){
            DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Error when reading host database");
            return false;
        }
        if (!buf.empty()){
            _database.insert({buf,100});
        }
    }
    file.close();

    // Get feed namme from file name
    buf = darwin::files_utils::GetNameFromPath(filename); // Filename already proven valid
    darwin::files_utils::ReplaceExtension(buf, "");
    this->_feed_name = buf;

    return true;
}

bool Generator::LoadJsonFile(const std::string& filename) {
    DARWIN_LOGGER;
    std::ifstream file(filename.c_str());
    rapidjson::Document database;

    if (!file) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
        return false;
    }
    DARWIN_LOG_DEBUG("HostlookupGenerator:: Parsing database...");
    rapidjson::IStreamWrapper isw(file);
    database.ParseStream(isw);
    if (not this->LoadJsonDatabase(database)) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool Generator::LoadJsonDatabase(const rapidjson::Document& database) {
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
        this->LoadJsonEntry(entry);
    }
    if (this->_database.size() == 0) {
        DARWIN_LOG_CRITICAL("HostlookupGenerator:: No usable entry in the database. Stopping.");
        return false;
    }
    return true;
}

bool Generator::LoadJsonEntry(const rapidjson::Value& entry) {
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
