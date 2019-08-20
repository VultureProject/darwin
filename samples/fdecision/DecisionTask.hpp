/// \file     DecisionTask.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     17/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <map>
#include <mutex>
#include "protocol.h"
#include "Session.hpp"
#include "Manager.hpp"

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/xxhash.h"
#include "../../toolkit/xxhash.hpp"

#define DARWIN_FILTER_DECISION 0x64656373

class DecisionTask : public darwin::Session {
public:
    // data_map[ip] -> flags
    typedef std::map<std::string, std::string> request_data_map_t;

public:
    DecisionTask(boost::asio::local::stream_protocol::socket& socket,
                 darwin::Manager& manager,
                 std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                 request_data_map_t* data,
                 std::mutex* mut);
    ~DecisionTask() override = default;

public:
    void operator()() override;

private:
    void Decide(const std::string &data);

    void SaveInfo(const std::string &data);

    bool ParseBody() override;

private:
    request_data_map_t* _data;
    std::mutex* _data_mutex;
    std::string _current_data;
    std::vector<std::string> _data_list;
};
