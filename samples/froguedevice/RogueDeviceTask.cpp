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
#include "FileManager.hpp"
#include "Logger.hpp"

RogueDeviceTask::RogueDeviceTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               PyObject *py_function,
                               std::string input_csv,
                               std::string output_csv,
                               std::string output_json)
        : Session{socket, manager, cache, cache_mutex}, _py_function(py_function),
        _csv_input_path{input_csv}, _csv_ouput_path{output_csv},
        _json_output_path{output_json} {
    _is_cache = _cache != nullptr;
}

void RogueDeviceTask::operator()() {
    DARWIN_LOGGER;

    if (RunScript()) {
        unlink(this->_csv_input_path.c_str()); // Deleting input csv
        std::string& response = this->LoadReponseFromFiles();// Loads & formats output
        this->SendFormatedResponse(response);
        this->_header.response = DARWIN_RESPONSE_SEND_NO; // We send response ourselve, no need to send an empty response after
        DARWIN_LOG_INFO("RogueDeviceTask:: Run succesful");
    } else {
        // ERROR
        DARWIN_LOG_INFO("RogueDeviceTask:: ");
    }
}

void SendFormatedResponse(const std::string& body) {
    DARWIN_LOGGER;
    darwin_filter_packet_t packet;
    std::size_t packet_size = sizeof(packet) + body.length();

    memset(&packet, 0, packet_size);
    packet.type = DARWIN_PACKET_FILTER;
    packet.response = this->_header.response;
    packet.certitude_size = 0;
    packet.filter_code = GetFilterCode();
    packet.body_size = body.length();
    memcpy(packet.evt_id, _header.evt_id, 16);

    boost::asio::async_write(this->_socket,
                        boost::asio::buffer(&packet, packet_size),
                        boost::bind(&Session::SendToClientCallback, this,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));
}

bool RogueDeviceTask::RunScript() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("RogueDeviceTask:: RunScript:: Converting strings to python objects");

    PyObject* py_csv_input = PyUnicode_FromString(this->_csv_input_path);
    PyObject* py_csv_ouput = PyUnicode_FromString(this->_csv_ouput_path);
    PyObject* py_json_ouput = PyUnicode_FromString(this->_json_ouput_path);
    PyObject* py_args = PyTuple_Pack(3, py_csv_input, py_csv_output, py_json_output);
    PyObject* py_return_value = nullptr; // the Python result, stored in a PyObject
    bool ret = true;
    int truthy = 0;

    if (not darwin::pythonutils::CallPythonFunction(this->_py_function, py_args, &py_return value)) {
        DARWIN_LOG_ERROR("RogueDeviceTask:: RunScript:: Something went wrong while calling the Python "
                         "function");
        ret = false;
    } else {
        if (truthy = PyObject_IsTrue(py_return_value) == -1) {
            DARWIN_LOG_ERROR("RogueDeviceTask:: RunScript:: Unable to get python function return");
            ret = false;
        } else {
            if (not truthy) {
                DARWIN_LOG_ERROR("RogueDeviceTask:: RunScript:: Something went wrong in the script and was handled gracefully");
                ret = false;
            }
        }
    }
    Py_DECREF(py_csv_input);
    Py_DECREF(py_csv_ouput);
    Py_DECREF(py_json_ouput);
    Py_DECREF(py_args);
    Py_DECREF(py_return_value);
    return ret;
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

        darwin::toolkit::FileManager file(_csv_input_path, false, false);
        if (not file.Open()) {
            DARWIN_LOG_CRITICAL("RogueDeviceTask:: Unable to open input csv file.");
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

bool RogueDeviceTask::ParseLine(rapidjson::Value& line,
                                darwin::toolkit::FileManager& file) {
    // IP, HOSTNAME, OS, PROTO, PORT
    //  0     1       2    3      4
    // str   str     str  str    int

    std::stringstream ss;

    if (!line.IsArray()) {
        DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Entry must be a list. Ignoring entry...");
        return false;
    }

    auto items = request.GetArray();

    if (items.Size() != 5) {
        DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: You must provide exactly 11 arguments per entry. Ignoring entry...");
        return false;
    }

    ss << "IP,HOSTNAME,OS,PROTO,PORT" << std::endl;
    for (int i = 0; i < 5; ++i) {
        if (i == 4) /* PORT */ {
            if (not items[i].IsInt()) {
                DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Value is not an integer.");
                return false;
            }
            ss << items[i].GetInt();
        } else {
            if (not items[i].IsString()) {
                DARWIN_LOG_WARNING("RogueDeviceTask:: ParseLine: Value is not an integer.");
                return false;
            }
            ss << items[i].GetString() << ',';
        }
    }
    ss << std::endl;
    file << ss.str();
    return true;
}
