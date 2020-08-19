/// \file     BufferThreadManager.cpp
/// \authors  ggonzalez
/// \version  1.0
/// \date     28/05/20
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "../../toolkit/RedisManager.hpp"
#include "BufferThreadManager.hpp"
#include "BufferThread.hpp"
#include "Logger.hpp"

BufferThreadManager::BufferThreadManager(int nb_threads) : AThreadManager(nb_threads) {}

std::shared_ptr<AThread> BufferThreadManager::Start() {
    auto ptr = std::make_shared<BufferThread>(_connector);
    std::shared_ptr<AThread> res = std::static_pointer_cast<AThread>(ptr);
    return res;
}

void BufferThreadManager::SetConnector(std::shared_ptr<AConnector> connector) {
    this->_connector = connector;
}