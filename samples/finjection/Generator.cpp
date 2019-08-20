/// \file     Generator.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     06/12/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "../../toolkit/lru_cache.hpp"
#include "base/Logger.hpp"
#include "Generator.hpp"
#include "InjectionTask.hpp"

bool Generator::Configure(const std::string &model_path, const std::size_t cache_size) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Injection:: Generator:: Configuring...");

    LoadKeywords();
    if (!LoadClassifier(model_path)) {
        return false;
    }

    DARWIN_LOG_DEBUG("Generator:: Cache initialization. Cache size: " + std::to_string(cache_size));
    if (cache_size > 0) {
        _cache = std::make_shared<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>>(cache_size);
    }

    DARWIN_LOG_DEBUG("Injection:: Generator:: Configured");
    return true;
}

void Generator::LoadKeywords() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Injection:: LoadKeywords:: Loading keywords...");

    std::vector<std::string> malicious_path_traversal = {
            "\"", "%22", "%2522", "~", "%25", "%2525", "%00", "%2500", "%0D%0A", "%250D%0A", "%250D%250A",
            "..\\", "..%22", "%2E%2E%22", ".\\", ".%22", "%2E%22",
            "./", ".%2F", "%2E/", "%2E%2F", "%252E%252F", "../../", "..%2F..%2F", "%2E%2E%2F%2E%2E%2F",
            "//", "%2F%2F", "%2F2F", "\\", "%22%22", "%2522%2522",
            "../", "..%2F", "%2E%2E/", "%2E%2E%2F", "%252E%252E%252F"
    };

    std::vector<std::string> malicious_sqli = {
            "%25", "%2525", " OR ", "%20OR%20", "%2520OR%2520", " AND ", "%20AND%20", "%2520AND%2520",
            "UNION", "SELECT", "SLEEP(", "SLEEP%28", "SLEEP%2528", "HAVING", "GROUP BY", "GROUP%20BY", "GROUP%2520BY", "FROM", "LIMIT",
            "/*", "%2F%2A", "%252F%252A", "*/", "%2A%2F", "%252A%252F", "CONCAT", "REVERSE(", "REVERSE%28", "REVERSE%2528",
            "DISTINCT", "UNHEX(", "UNHEX%28", "UNHEX%2528", "SCRIPT"
    };

    std::vector<std::string> malicious_xss = {
            "<", "%3C", "%253C", ">", "%3E", "%253E", "{", "%7B", "%257B", "}", "%7D", "%257D",
            "<!-", "%3C%21-", "%3C%21%2D", "%253C%2521%252D", "->", "-%3E", "%2D%3E", "%252D%253E",
            "-- ", "--%20", "DOCUMENT.COOKIE", "DOCUMENT%2ECOOKIE", "ALERT",
            "|", "%7C", "%257C", "^", "%5E", "%253E", "[", "%5B", "%255B", "]", "%5D", "%255D",
            "`", "%60", "%2560", "'", "%27", "%2527", "SCRIPT", "JAVASCRIPT", "EVAL", "XSS",
            "PASSWORD", "PASSWD", "ETC/", "ETC%2F", "WINNT"
    };

    _keywords.insert(malicious_path_traversal.begin(), malicious_path_traversal.end());
    _keywords.insert(malicious_sqli.begin(), malicious_sqli.end());
    _keywords.insert(malicious_xss.begin(), malicious_xss.end());
}

bool Generator::LoadClassifier(const std::string &model_path) {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("Generator:: LoadClassifier:: Loading classifier at '" + model_path + "'...");

    if (XGBoosterCreate(0, 0, &_booster) == -1) {
        DARWIN_LOG_CRITICAL("Generator:: LoadClassifier:: XGBoosterCreate:: '" + std::string(XGBGetLastError()) + "'");
    }


    if (XGBoosterLoadModel(_booster, model_path.c_str()) == -1) {
        DARWIN_LOG_CRITICAL("Generator:: LoadClassifier:: XGBoosterLoadModel:: '" + std::string(XGBGetLastError()) + "'");

        return false;
    }
    DARWIN_LOG_DEBUG("Generator:: LoadClassifier:: Classifier loaded");

    return true;
}

darwin::session_ptr_t
Generator::CreateTask(boost::asio::local::stream_protocol::socket& socket,
                      darwin::Manager& manager) noexcept {
    return std::static_pointer_cast<darwin::Session>(
            std::make_shared<InjectionTask>(socket, manager, _cache, _booster, &_keywords,
                                            1,
                                            _keywords.size() + 1));
}

Generator::~Generator() {
    DARWIN_LOGGER;
    if (_booster == nullptr) {
        DARWIN_LOG_DEBUG("Injection:: ~Generator:: No Booster to free");
        return;
    }


    if (XGBoosterFree(_booster) == -1) {
        DARWIN_LOG_DEBUG("Injection:: ~Generator:: Booster could not be freed: '" +
                  std::string(XGBGetLastError()) + "'");
    } else {
        DARWIN_LOG_DEBUG("Injection:: ~Generator:: Booster freed");
        _booster = nullptr;
    }
}