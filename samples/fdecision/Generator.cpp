/// \file     Generator.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     18/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <memory>
#include "Generator.hpp"
#include "DecisionTask.hpp"
#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"

bool Generator::Configure(std::string const& configFile, const std::size_t cache_size) {
    DARWIN_LOGGER;
    (void) configFile;

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    return true;
}

darwin::session_ptr_t Generator::CreateTask(
        boost::asio::local::stream_protocol::socket& socket,
        darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<DecisionTask>(socket, manager, _cache, &data, &data_mutex));
}