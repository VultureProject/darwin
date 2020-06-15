/// \file     fSofaConnector.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     03/06/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include "AConnector.hpp"

class fSofaConnector : public AConnector {
    public:
    fSofaConnector(std::string filter_socket_path, int interval, std::string redis_list, unsigned int minLogLen);
    virtual ~fSofaConnector() override final = default;

    public:
    virtual bool sendData(std::map<std::string, std::string> input_line) override final;

    private:
};