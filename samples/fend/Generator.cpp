/// \file     Generator.cpp
/// \authors  jjourdin
/// \version  1.0
/// \date     22/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "base/Logger.hpp"
#include "Generator.hpp"
#include "EndTask.hpp"

#include <fstream>
#include <string>

#include "../../toolkit/RedisManager.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("End:: Generator:: Configuring...");

    _redis_socket_path = configFile;

    DARWIN_LOG_DEBUG("ConnectionSupervision:: Generator:: Configured");
    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<EndTask>(socket, manager, _cache, _redis_socket_path));
}

Generator::~Generator() = default;
