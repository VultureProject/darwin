/// \file     ThreadGroup.hpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     15/10/19
/// \license  GPLv3
/// \brief    Copyright (c) 2019 Advens. All rights reserved.

#pragma once

#include <thread>
#include <forward_list>
#include <memory>

/// \namespace darwin
namespace darwin {
    
    /// Class used to manage a group of std::thread.
    ///
    /// \class ThreadGroup
    class ThreadGroup {
    protected:
        std::forward_list<std::unique_ptr<std::thread>> _thread_list;

    public:

        /// Default constructor.
        ThreadGroup() = default;

        /// Destructor.
        /// If the threads were not joined, join all and free ressources.
        ~ThreadGroup();

        // You cannot copy a ThreadGroup
        ThreadGroup(const ThreadGroup&) = delete;
        ThreadGroup(const ThreadGroup&&) = delete;
        ThreadGroup& operator=(const ThreadGroup&) = delete;
        ThreadGroup& operator=(const ThreadGroup&&) = delete;

        /// Create a thread and place it inside the group.
        ///
        /// \param args Arguments to pass to the thread constructor. (See std::thread constructor documentation)
        template <class... Args>
        void CreateThread(Args&&... args) {
            _thread_list.emplace_front(std::make_unique<std::thread>(args...));
        }

        /// Join all the thread currently in the group.
        /// Then remove them from the group.
        void JoinAll();
    };
}