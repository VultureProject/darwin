/// \file     Generator.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     03/12/2013
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <fstream>
#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "TestTask.hpp"
#include "tsl/hopscotch_map.h"
#include "tsl/hopscotch_set.h"


bool Generator::LoadConfig(const rapidjson::Document &configuration) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Test:: Generator:: Loading Configuration...");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<TestTask>(socket, manager, _cache, _cache_mutex));
}
