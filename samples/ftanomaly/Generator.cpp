/// \file     Generator.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     01/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "base/Logger.hpp"
#include "base/Core.hpp"
#include "Generator.hpp"
#include "TAnomalyTask.hpp"
#include "TAnomalyThreadManager.hpp"

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"

bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("TAnomaly:: Generator:: Loading configuration...");

    // Generating name for the internaly used redis set
    _redis_internal = darwin::Core::instance().GetFilterName() + REDIS_INTERNAL_LIST;

    _anomaly_thread_manager = std::make_shared<AnomalyThreadManager>(_redis_internal);
    if(!_anomaly_thread_manager->Start()) {
        DARWIN_LOG_CRITICAL("TAnomaly:: Generator:: Error when starting polling thread");
        return false;
    }

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<AnomalyTask>(socket, manager, _cache, _cache_mutex,
                                          _anomaly_thread_manager, _redis_internal));
}