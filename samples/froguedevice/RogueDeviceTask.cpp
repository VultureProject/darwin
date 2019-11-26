/// \file     RogueDevice.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     25/11/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string>
#include <sstream>

#include "../toolkit/rapidjson/document.h"
#include "RogueDeviceTask.hpp"
#include "Logger.hpp"

RogueDeviceTask::RogueDeviceTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               PyObject *py_function)
        : Session{socket, manager, cache, cache_mutex}, _py_function(py_function) {
    _is_cache = _cache != nullptr;
}

void RogueDeviceTask::operator()() {
    DARWIN_LOGGER;

    // We have a generic hash function, which takes no arguments as these can be of very different types depending
    // on the nature of the filter
    // So instead, we set an attribute corresponding to the current host being processed, to compute the hash
    // accordingly
    for (const int fahrenheit_temp : _fahrenheit_temps) {
        SetStartingTime();
        _current_fahrenheit_temp = fahrenheit_temp;
        unsigned int certitude;
        xxh::hash64_t hash;

        if (_is_cache) {
            hash = GenerateHash();

            if (GetCacheResult(hash, certitude)) {
                _certitudes.push_back(certitude);
                DARWIN_LOG_DEBUG("RogueDeviceTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = WarmWeatherDetector(fahrenheit_temp);
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("RogueDeviceTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }
}

unsigned int RogueDeviceTask::WarmWeatherDetector(const int fahrenheit_temperature) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("RogueDeviceTask:: WarmWeatherDetector:: Checking if the temperature is above " +
                     std::to_string(KELVIN_TEMP_THRESHOLD) + " degrees Kelvin");

    PyObject *py_fahrenheit_temp = PyLong_FromLong((long) fahrenheit_temperature);
    PyObject* py_args = PyTuple_Pack(1, py_fahrenheit_temp);
    PyObject *py_kelvin_temp = nullptr; // the Python result, stored in a PyObject
    unsigned int certitude = 0; // the default certitude
    int kelvin_temp = 0; // the Kelvin temperature obtained (from the PyObject result instance)

    if (!darwin::pythonutils::CallPythonFunction(_py_function, py_args, &py_kelvin_temp)) {
        DARWIN_LOG_ERROR("RogueDeviceTask:: WarmWeatherDetector:: Something went wrong while calling the Python "
                         "function");

        certitude = DARWIN_ERROR_RETURN;
    } else {
        kelvin_temp = (int) PyLong_AsLong(py_kelvin_temp);

        DARWIN_LOG_DEBUG("With a temperature of " +
                         std::to_string(fahrenheit_temperature) +
                         " degrees Fahrenheit, the corresponding temperature in degrees Kelvin is " +
                         std::to_string(kelvin_temp));

        if (kelvin_temp > KELVIN_TEMP_THRESHOLD) {
            certitude = 100;
            DARWIN_LOG_DEBUG("Kelvin temperature is above threshold: setting Darwin certitude to 100");
        } else {
            DARWIN_LOG_DEBUG("Kelvin temperature is below or equal to threshold: setting Darwin certitude to 0");
        }
    }

    DARWIN_LOG_DEBUG("Certitude obtained is " + std::to_string(certitude));
    return certitude;
}

bool RogueDeviceTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("RogueDeviceTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("RogueDeviceTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("RogueDeviceTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            this->ParseLine(request);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("RogueDeviceTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}

bool RogueDeviceTask::ParseLine(rapidjson::Value& line) {
    // IP, HOSTNAME, OS, PROTO, PORT, SERVICE, PRODUCT, SERVICE_FP, NSE_SCRIPT_ID, NSE_SCRIPT_OUTPUT, NOTES
    //  0     1       2    3      4      5        6          7             8               9           11
    // str   str     str  str    int    str      str        str           str             str          str

    std::stringstream ss;

    if (!line.IsArray()) {
        DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Entry must be a list. Ignoring entry...");
        return false;
    }

    auto items = request.GetArray();

    if (items.Size() != 11) {
        DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: You must provide exactly 11 arguments per entry. Ignoring entry...");
        return false;
    }

    for (int i = 0; i < 11; ++i) {
        if (i == 4) /* PORT */ {
            if (not items[i].IsInt()) {
                DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Value is not an integer.");
                return false;
            }
        } else {
            if (not items[i].IsString()) {
                DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Value is not an integer.");
                return false;
            }
        }
        ss << items[i];
    }

    return true;
}
