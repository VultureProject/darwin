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

        /// Extract the file name from the complete file path
        ///
        /// \param filename A string containing the complete file path
        /// \return A string contaning the extracted file name. Empty not found.
        std::string GetNameFromPath(const std::string& filename);

        /// Replace the file extension.
        /// If no extension found no action performed.
        ///
        /// \param filename The original filename. This agrument is modified.
        /// \param new_extension The new extension to applys to the filename
        void ReplaceExtension(std::string& filename, const std::string& new_extension);
    }
}
