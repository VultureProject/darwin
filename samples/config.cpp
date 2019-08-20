/// \file     config.cpp
/// \authors  nsanti
/// \version  1.0
/// \date     11/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "config.hpp"

namespace darwin {
    namespace config{

        // The map that associate a representative string to an output_type
        std::map<std::string, output_type> output_map = {{"RAW", RAW},{"LOG", LOG},{"NONE", NONE},{"PARSED", PARSED}};

        output_type convert_output_string(const std::string &output){

            output_type res;

            try {
                res = output_map.at(output);
            }
            catch (const std::out_of_range& e) {
                res = output_type::NONE;
            }

            return res;
        }
    }
}
