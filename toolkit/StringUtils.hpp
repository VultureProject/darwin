/// \file     StringUtils.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     06/08/20
/// \license  GPLv3
/// \brief    Copyright (c) 2020 Advens. All rights reserved.

#pragma once

#include <string>
#include <vector>

namespace darwin {
    namespace strings {
        ///\brief Small Util to split a string into a vector of string on char delim
        ///
        ///\param source The string to split
        ///\param delim The char on which to split (will NOT be included anywere in the result)
        ///
        ///\return The newly created vector of strings
        std::vector<std::string> SplitString(const std::string& source, char delim);
    } // namespace strings
} // namespace darwin
