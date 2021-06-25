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

        ///\brief Util to parse a 'long' from a char array, it returns true if it works, 
        ///       false if it couldn't parse the number, 'str' must be null terminated just after the number :
        ///        -> "70\0" -> true
        ///        -> "70Hi\0" -> false, 'out' is not modified
        ///
        ///\param str source to parse (must be null terminated, it will not be checked)
        ///\param out a reference to a 'long' which will be set by the function
        ///
        ///\return true if the parsing works, false otherwise
        bool StrToLong(const char * str, long& out);
    } // namespace strings
} // namespace darwin
