/// \file     SofaTask.cpp
/// \authors  Hugo Soszynski
/// \version  1.0
/// \date     25/11/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include <string>
#include <sstream>
#include <fstream>
#include <boost/bind.hpp>

#include "../toolkit/rapidjson/document.h"
#include "SofaTask.hpp"
#include "FileManager.hpp"
#include "Logger.hpp"

SofaTask::SofaTask(boost::asio::local::stream_protocol::socket& socket,
                               darwin::Manager& manager,
                               std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                               std::mutex& cache_mutex,
                               PyObject *py_function,
                               std::string input_csv,
                               std::string output_csv,
                               std::string output_json)
        : Session{"sofa", socket, manager, cache, cache_mutex}, _py_function(py_function),
        _csv_input_path{input_csv}, _csv_output_path{output_csv},
        _json_output_path{output_json} {
    _is_cache = _cache != nullptr;
}

SofaTask::~SofaTask() {
    // Just in case
    unlink(this->_csv_input_path.c_str()); // Deleting input csv
    unlink(this->_csv_output_path.c_str()); // Deleting output csv
    unlink(this->_json_output_path.c_str()); // Deleting output json
}

long SofaTask::GetFilterCode() noexcept {
    return DARWIN_FILTER_SOFA;
}

void SofaTask::operator()() {
    DARWIN_LOGGER;
    std::string response;

    if (this->RunScript() and this->LoadResponseFromFile()) {
        this->_header.response = DARWIN_RESPONSE_SEND_BACK; // We send response only to caller
        DARWIN_LOG_INFO("SofaTask:: Run successful");
    } else {
        _response_body.clear();
        DARWIN_LOG_INFO("SofaTask:: Error occured during script run");
    }
    unlink(this->_csv_input_path.c_str()); // Deleting input csv
    unlink(this->_csv_output_path.c_str()); // Deleting output csv
    unlink(this->_json_output_path.c_str()); // Deleting output json
}

bool SofaTask::LoadResponseFromFile() {
    DARWIN_LOGGER;
    std::ifstream csv(this->_csv_output_path, std::ifstream::in);
    char buffer[4096];

    _response_body.clear();
    if (not csv.is_open()) {
        DARWIN_LOG_ERROR("SofaTask:: Unable to open csv result file.");
        return false;
    }
    while (csv.good()) {
        std::memset(buffer, 0, 4096);
        csv.read(buffer, 4096);
        _response_body.append(buffer, csv.gcount());
    }
    if (not csv.eof() and csv.fail()) {
        DARWIN_LOG_ERROR("SofaTask:: Fail during file reading.");
        return false;
    }
    return true;
}

bool SofaTask::SendToClient() noexcept {
    DARWIN_LOGGER;
    std::size_t packet_size = sizeof(darwin_filter_packet_t) + _response_body.length();
    DARWIN_LOG_DEBUG("SofaTask::SendToClient:: Packet size is: " + std::to_string(packet_size) + "; Body size is: " + std::to_string(_response_body.length()));

    darwin_filter_packet_t* response_packet = reinterpret_cast<darwin_filter_packet_t*>(malloc(packet_size));
    if (response_packet == nullptr) {
        DARWIN_LOG_ERROR("SofaTask::SendToClient:: Unable to allocate response packet memory");
        return false;
    }

    DARWIN_LOG_DEBUG("SofaTask::SendToClient:: Building response packet");
    memset(response_packet, 0, packet_size);
    response_packet->type = DARWIN_PACKET_FILTER;
    response_packet->response = this->_header.response;
    response_packet->certitude_size = 0;
    response_packet->filter_code = GetFilterCode();
    response_packet->body_size = _response_body.length();
    memcpy(response_packet->evt_id, this->_header.evt_id, 16);
    memcpy((char*)(response_packet) + sizeof(darwin_filter_packet_t), _response_body.c_str(), _response_body.length());

    DARWIN_LOG_DEBUG("SofaTask::SendToClient:: Sending response packet async");
    boost::asio::async_write(this->_socket,
                        boost::asio::buffer(response_packet, packet_size),
                        boost::bind(&SofaTask::SendToClientCallback, this,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred));
    DARWIN_LOG_DEBUG("SofaTask::SendToClient:: Async response sending done");
    free(response_packet);
    return true;
}

bool SofaTask::RunScript() noexcept {
    DARWIN_LOGGER;
    DARWIN_LOG_DEBUG("SofaTask:: RunScript:: Converting strings to python objects");

    PyObject* py_csv_input = PyUnicode_FromString(this->_csv_input_path.c_str());
    PyObject* py_csv_output = PyUnicode_FromString(this->_csv_output_path.c_str());
    PyObject* py_json_output = PyUnicode_FromString(this->_json_output_path.c_str());
    PyObject* py_args = PyTuple_Pack(3, py_csv_input, py_csv_output, py_json_output);
    PyObject* py_return_value = nullptr; // the Python result, stored in a PyObject
    bool ret = true;
    int truthy = 0;

    if (not darwin::pythonutils::CallPythonFunction(this->_py_function, py_args, &py_return_value)) {
        DARWIN_LOG_ERROR("SofaTask:: RunScript:: Something went wrong while calling the Python "
                         "function");
        ret = false;
    } else {
        DARWIN_LOG_DEBUG("SofaTask:: RunScript:: Python Function Call returned true");
        if ((truthy = PyObject_IsTrue(py_return_value)) == -1) {
            DARWIN_LOG_ERROR("SofaTask:: RunScript:: Unable to get python function return");
            ret = false;
        } else {
            if (not truthy) {
                DARWIN_LOG_ERROR("SofaTask:: RunScript:: Something went wrong in the script and was handled gracefully");
                ret = false;
            }
        }
    }
    Py_DECREF(py_csv_input);
    Py_DECREF(py_csv_output);
    Py_DECREF(py_json_output);
    Py_DECREF(py_args);
    Py_DECREF(py_return_value);
    return ret;
}

bool SofaTask::ParseBody() {
    DARWIN_LOGGER;

    try {
        _body.Parse(_raw_body.c_str());

        if (!_body.IsArray()) {
            DARWIN_LOG_ERROR("SofaTask:: ParseBody: You must provide a list");
            return false;
        }

        auto values = _body.GetArray();

        if (values.Size() <= 0) {
            DARWIN_LOG_ERROR("SofaTask:: ParseBody: The list provided is empty");
            return false;
        }

        darwin::toolkit::FileManager file(_csv_input_path, false, false);
        if (not file.Open()) {
            DARWIN_LOG_CRITICAL("SofaTask:: Unable to open input csv file.");
            return false;
        }

        file << "IP,HOSTNAME,OS,PROTO,PORT\n";
        for (auto &request : values) {
            this->ParseLine(request, file);
        }
    } catch (...) {
        DARWIN_LOG_CRITICAL("SofaTask:: ParseBody: Unknown Error");
        return false;
    }
    DARWIN_LOG_DEBUG("SofaTask:: ParseBody:: parsed body : " + _raw_body);
    return true;
}

bool SofaTask::ParseLine(rapidjson::Value& line,
                                darwin::toolkit::FileManager& file) {
    // IP, HOSTNAME, OS, PROTO, PORT
    //  0     1       2    3      4
    // str   str     str  str    str
    DARWIN_LOGGER;
    std::stringstream ss;

    if (!line.IsArray()) {
        DARWIN_LOG_WARNING("SofaTask:: ParseLine: Entry must be a list. Ignoring entry...");
        return false;
    }

    auto items = line.GetArray();

    if (items.Size() != 5) {
        DARWIN_LOG_WARNING("SofaTask:: ParseLine: You must provide exactly 11 arguments per entry. Ignoring entry...");
        return false;
    }

    for (int i = 0; i < 5; ++i) {
        if (not items[i].IsString()) {
            DARWIN_LOG_WARNING("SofaTask:: ParseLine: Value is not an integer.");
            return false;
        }
        auto item = items[i].GetString();
        size_t j = 0;
        bool quotes = false;
        while (item[j] != '\0') {
            if (item[j] == ',') {
                quotes = true;
                break;
            }
            j++;
        }
        if (not quotes)
            ss << item;
        else
            ss << '"' << item << '"';
        if (i < 4)
            ss << ',';
    }
    ss << std::endl;
    file << ss.str();
    return true;
}

bool SofaTask::ParseLine(rapidjson::Value& line) {
    (void)line;
    return false;
}
