/// \file     enums.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     03/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

typedef enum valueType_e {
    STRING = 0,
    INT,
    DOUBLE,
    FLOAT,
    UNKNOWN
} valueType;

typedef enum outputType_e {
    ANOMALY = 0,
    SOFA,
} outputType;

