/// \file     Files.hpp
/// \authors  tbertin
/// \version  1.0
/// \date     04/09/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

/// \namespace darwin
namespace darwin {
    /// \namespace validator
    namespace files_utils {
        std::istream& GetLineSafe(std::istream& is, std::string& t);
    }
}
