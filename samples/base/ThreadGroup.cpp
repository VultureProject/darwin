/// \file     ThreadGroup.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     15/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#include "ThreadGroup.hpp"

namespace darwin {

    ThreadGroup::~ThreadGroup() {
        if (!this->_thread_list.empty())
            this->JoinAll();
    }

    void ThreadGroup::JoinAll() {
        while (!this->_thread_list.empty()) {
            std::unique_ptr<std::thread> t = std::move(this->_thread_list.front());
            t->join();
            this->_thread_list.pop_front();
        }
    }

}