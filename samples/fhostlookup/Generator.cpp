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

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Configuring...");

    std::ifstream file(configFile.c_str());
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string host ;
        if (!(iss >> host)) {
            DARWIN_LOG_CRITICAL("HostLookup:: Generator:: Configure:: Cannot open host database");
            return false;
        }
        _database.insert({host,0});
    }

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("HostLookup:: Generator:: Configured");
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<HostLookupTask>(socket, manager, _cache, _database));
}

Generator::~Generator() {
}