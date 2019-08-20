#pragma once

#include <map>
#include <string>

namespace darwin{
    namespace config {

/// Represent the type of output the filter
/// will send to the next filter
///
/// \enum output_type
        enum output_type {
            RAW, //!< Send the unparsed body the filter have received
            LOG, //!< Send result
            NONE,//!< Send nothing to the next filter
            PARSED, //!< Send the body parsed by the filter
        };

/// Return the output_type associated with the string given
/// If the string given is not valid, return the output_type NONE
///
/// \param output the string we want to convert
/// \return the output_type associated
        output_type convert_output_string(const std::string &output);
    }
}
