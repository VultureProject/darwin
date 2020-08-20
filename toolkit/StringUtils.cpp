/// \file     StringUtils.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     06/08/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include <sstream>

#include "StringUtils.hpp"

std::vector<std::string> darwin::strings::SplitString(const std::string& source, char delim) {
    std::vector<std::string> res;

    std::istringstream stream(source);
    std::string str;

    while (getline(stream, str, delim))
        res.emplace_back(str);
    return res;
}
