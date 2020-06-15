/// \file     BufferTask.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     29/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include <map>
#include <vector>

#include "Logger.hpp"
#include "enums.hpp"

class AConnector {
    public: //Constructors & Destructor
    AConnector(outputType filter_type, std::string filter_socket_path, int interval, std::string redis_list, unsigned int minLogLen);
    virtual ~AConnector() = default;

    public:
    int getInterval() const;
    unsigned int getRequiredLogLength() const;
    std::string getRedisList() const;
    /// Add the _entry parsed to REDIS
    /// \return true on success, false otherwise.
    bool REDISAddEntry();
    bool parseData(std::string fildname);

    public: // To be implemented by children
    virtual bool sendData(std::map<std::string, std::string> input_line) = 0;

    protected:
    outputType _filter_type; // Used to link with the correct task
    std::string _filter_socket_path; // Socket filepath on which to write data every _interval seconds if there is enough data in Redis 
    int _interval = 300; // Interval in seconds between two reads on Redis (default 5min = 300s)
    std::string _redis_list;
    std::string _entry;
    std::map<std::string, std::string> _input_line;
    unsigned int _minLogLen;

    private:// Childs will have to implement there own datas to transfert to their filter
};