/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     10/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "ReputationTask.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;

    DARWIN_LOG_DEBUG("Reputation:: Generator:: Configuring...");

    auto status = MMDB_open(configFile.c_str(), MMDB_MODE_MMAP, &_database);
    if (status != MMDB_SUCCESS) {
        DARWIN_LOG_CRITICAL("Reputation:: Generator:: Configure:: Cannot open database");
        return false;
    }

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("Reputation:: Generator:: Configured");
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<ReputationTask>(socket, manager, _cache, &_database));
}

Generator::~Generator() {
    MMDB_close(&_database);
}