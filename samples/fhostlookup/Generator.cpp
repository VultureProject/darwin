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


bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Loading classifier...");
    std::string line, db;

    if (!configuration.HasMember("database")) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Missing parameter: \"database\"");
        return false;
    }

    if (!configuration["database"].IsString()) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: \"database\" needs to be a string");
        return false;
    }

    db = configuration["database"].GetString();
    std::ifstream file(db.c_str());
    std::string host;

    if (!file) {
        DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
        return false;
    }

    while (!darwin::files_utils::GetLineSafe(file, host).eof()) {
        if(file.fail() or file.bad()){
            DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Error when reading host database");
            return false;
        }
        if (!host.empty()){
            _database.insert({host,0});
        }
    }

    file.close();
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<HostLookupTask>(socket, manager, _cache, _cache_mutex, _database));
}
