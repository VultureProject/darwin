/// \file     Manager.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     05/07/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <set>
#include "Session.hpp"

namespace darwin {
    class Manager {
    public:
        Manager() = default;

        ~Manager() = default;

        // Make the manager non copyable & non movable
        Manager(Manager const&) = delete;

        Manager(Manager const&&) = delete;

        Manager& operator=(Manager const&) = delete;

        Manager& operator=(Manager const&&) = delete;

    public:
        /// Add the specified session to the manager and start it.
        void Start(session_ptr_t c);

        /// Stop the specified session.
        void Stop(session_ptr_t c);

        /// Stop all sessions.
        void StopAll();

    private:
        std::set<session_ptr_t> _sessions; //!< The managed sessions.
    };
}