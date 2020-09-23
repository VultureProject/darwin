/// \file     uuid.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     12/08/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <vector>

namespace darwin {
    namespace uuid {
        std::vector<char> GenUuid();
    }
}