/// \file     Stats.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     25/11/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "Stats.hpp"

namespace darwin {

    namespace stats {

        std::atomic<FilterStatusEnum> filter_status;
        std::atomic_uint_fast64_t clientsNum;
        std::atomic_uint_fast64_t received;
        std::atomic_uint_fast64_t parseError;
        std::atomic_uint_fast64_t matchCount;
    }
}
