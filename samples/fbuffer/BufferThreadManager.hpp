/// \file     ThreadManager.hpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>
#include <thread>

#include "Connectors.hpp"
#include "AThreadManager.hpp"
#include "AThread.hpp"

class BufferThreadManager : public AThreadManager {
    public:
    BufferThreadManager(int nb_thread);
    virtual ~BufferThreadManager() = default;

    public:
    virtual std::shared_ptr<AThread> Start() override final;
    void setConnector(std::shared_ptr<AConnector> connector);

    private:
    std::shared_ptr<AConnector> _connector;
};