/// \file     enums.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     03/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

namespace darwin
{
        typedef enum valueType_e {
        STRING = 0,
        INT,
        UNKNOWN_VALUE_TYPE
    } valueType;
    
    typedef enum outputType_e {
        ANOMALY = 0,
        SOFA,
        UNKNOWN_OUTPUT
    } outputType;
} // namespace darwin