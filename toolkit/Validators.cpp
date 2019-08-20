/// \file     Validators.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     02/05/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <regex>

#include "Logger.hpp"
#include "Validators.hpp"

/// \namespace darwin
namespace darwin {
    /// \namespace validator
    namespace validator {
        bool IsDomainValid(const std::string& domain) {
            DARWIN_LOGGER;

            // Regex taken from https://validators.readthedocs.io/en/latest/_modules/validators/domain.html#domain
            std::regex domain_regex = std::regex(
                    R"(^(([a-zA-Z]{1})|([a-zA-Z]{1}[a-zA-Z]{1})|([a-zA-Z]{1}[0-9]{1})|([0-9]{1}[a-zA-Z]{1})|([a-zA-Z0-9][-_.a-zA-Z0-9]{0,61}[a-zA-Z0-9]))\.([a-zA-Z]{2,13}|[a-zA-Z0-9-]{2,30}.[a-zA-Z]{2,3})$)"
            );

            std::smatch match;

            bool is_valid = std::regex_search(domain.begin(), domain.end(), match, domain_regex);

            if (is_valid) {
                DARWIN_LOG_DEBUG("The domain \"" + domain + "\" is valid");
            } else {
                DARWIN_LOG_DEBUG("The domain \"" + domain + "\" is not valid");
            }

            return is_valid;
        }
    }
}
