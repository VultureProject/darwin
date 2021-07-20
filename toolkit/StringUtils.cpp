/// \file     StringUtils.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     06/08/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#include <sstream>
#include <cstdlib>
#include <climits>

#include "StringUtils.hpp"

std::vector<std::string> darwin::strings::SplitString(const std::string& source, char delim) {
    std::vector<std::string> res;

    std::istringstream stream(source);
    std::string str;

    while (getline(stream, str, delim))
        res.emplace_back(str);
    return res;
}

bool darwin::strings::StrToLong(const char * str, long& out) {
    char *end;
    long  l;
    errno = 0;
    l = strtol(str, &end, 10);

    if (
        ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX)
        || ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN)
        || (*str == '\0' || *end != '\0')
        )
    {
        // Overflow, underflow or Error while parsing a number
        return false;
    }

    out = l;
    return true;
}
