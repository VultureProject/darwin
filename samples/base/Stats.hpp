/// \file     Stats.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     25/11/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <atomic>
#include <string>

namespace darwin {

    namespace stats {
        enum class FilterStatusEnum {starting, configuring, running, stopping};

        extern std::atomic<FilterStatusEnum> filter_status;
        extern std::atomic_uint_fast64_t clientsNum;
        extern std::atomic_uint_fast64_t received;
        extern std::atomic_uint_fast64_t parseError;
        extern std::atomic_uint_fast64_t matchCount;
    }
}

#define SET_FILTER_STATUS(status) darwin::stats::filter_status.store(status)
#define STAT_CLIENT_INC darwin::stats::clientsNum++
#define STAT_CLIENT_DEC darwin::stats::clientsNum--
#define STAT_INPUT_INC darwin::stats::received++
#define STAT_PARSE_ERROR_INC darwin::stats::parseError++
#define STAT_MATCH_INC darwin::stats::matchCount++

#define STAT_FILTER_STATUS darwin::stats::filter_status
#define STAT_CLIENTS_NUM darwin::stats::clientsNum
#define STAT_INPUTS darwin::stats::received
#define STAT_PARSE_ERRORS darwin::stats::parseError
#define STAT_MATCHES darwin::stats::matchCount
