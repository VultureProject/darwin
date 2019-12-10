/// \file     Manager.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     05/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include "Logger.hpp"
#include "Stats.hpp"
#include "Manager.hpp"

namespace darwin {

    void Manager::Start(darwin::session_ptr_t c) {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("Manager::Start:: Starting new session...");
        {
            std::unique_lock<std::mutex> lck(this->_mutex);
            _sessions.insert(c);
        }
        STAT_CLIENT_INC;
        c->Start();
    }

    void Manager::Stop(darwin::session_ptr_t c) {
        {
            std::unique_lock<std::mutex> lck(this->_mutex);
            _sessions.erase(c);
        }
        STAT_CLIENT_DEC;
        c->Stop();
    }

    void Manager::StopAll() {
        {
            std::unique_lock<std::mutex> lck(this->_mutex);
            for (auto& c: _sessions)
                c->Stop();
            _sessions.clear();
        }
    }

}