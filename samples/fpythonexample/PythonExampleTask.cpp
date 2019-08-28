/// \file     PythonExample.cpp
/// \authors  gcatto
/// \version  1.0
/// \date     23/07/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string.h>

#include "../toolkit/rapidjson/document.h"
#include "PythonExampleTask.hpp"
#include "Logger.hpp"

PythonExampleTask::PythonExampleTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               PyObject *py_function)
        : Session{socket, manager, cache}, _py_function(py_function) {
    _is_cache = _cache != nullptr;
}

xxh::hash64_t PythonExampleTask::GenerateHash() {
    return xxh::xxhash<64>(std::to_string(_current_fahrenheit_temp));
}

void PythonExampleTask::operator()() {
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
                DARWIN_LOG_DEBUG("PythonExampleTask:: processed entry in "
                                 + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
                continue;
            }
        }

        certitude = WarmWeatherDetector(fahrenheit_temp);
        _certitudes.push_back(certitude);

        if (_is_cache) {
            SaveToCache(hash, certitude);
        }
        DARWIN_LOG_DEBUG("PythonExampleTask:: processed entry in "
                         + std::to_string(GetDurationMs()) + "ms, certitude: " + std::to_string(certitude));
    }

    Workflow();
    _fahrenheit_temps = std::vector<int>();
}

void PythonExampleTask::Workflow() {
    switch (header.response) {
        case DARWIN_RESPONSE_SEND_BOTH:
            SendToDarwin();
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_BACK:
            SendResToSession();
            break;
        case DARWIN_RESPONSE_SEND_DARWIN:
            SendToDarwin();
            break;
        case DARWIN_RESPONSE_SEND_NO:
        default:
            break;
    }
}

unsigned int PythonExampleTask::WarmWeatherDetector(const int fahrenheit_temperature) noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("PythonExampleTask:: WarmWeatherDetector:: Checking if the temperature is above " +
                     std::to_string(KELVIN_TEMP_THRESHOLD) + " degrees Kelvin");

    PyObject *py_fahrenheit_temp = PyLong_FromLong((long) fahrenheit_temperature);
    PyObject* py_args = PyTuple_Pack(1, py_fahrenheit_temp);
    PyObject *py_kelvin_temp = nullptr; // the Python result, stored in a PyObject
    unsigned int certitude = 0; // the default certitude
    int kelvin_temp = 0; // the Kelvin temperature obtained (from the PyObject result instance)

    if (!darwin::pythonutils::CallPythonFunction(_py_function, py_args, &py_kelvin_temp)) {
        DARWIN_LOG_ERROR("PythonExampleTask:: WarmWeatherDetector:: Something went wrong while calling the Python "
                         "function");

        certitude = 101;
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

bool PythonExampleTask::ParseBody() {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("PythonExampleTask:: ParseBody: " + body);

    try {
        rapidjson::Document document;
        document.Parse(body.c_str());

        if (!document.IsArray()) {
            DARWIN_LOG_ERROR("PythonExampleTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = document.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("PythonExampleTask:: ParseBody: The list provided is empty");
            return false;
        }

        for (auto &request : values) {
            if (!request.IsArray()) {
                DARWIN_LOG_ERROR("PythonExampleTask:: ParseBody: For each request, you must provide a list");
                return false;
            }

            auto items = request.GetArray();

            if (items.Size() != 1) {
                DARWIN_LOG_ERROR(
                    "PythonExampleTask:: ParseBody: You must provide exactly one argument per request: the "
                    "Fahrenheit temperature"
                );

                return false;
            }

            if (!items[0].IsInt()) {
                DARWIN_LOG_ERROR("PythonExampleTask:: ParseBody: The Fahrenheit temperature sent must be an integer");
                return false;
            }

            int fahrenheit_temp = items[0].GetInt();
            _fahrenheit_temps.push_back(fahrenheit_temp);
            DARWIN_LOG_DEBUG("PythonExampleTask:: ParseBody: Parsed element: " + std::to_string(fahrenheit_temp));
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("PythonExampleTask:: ParseBody: Unknown Error");
        return false;
    }

    return true;
}
