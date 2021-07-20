#pragma once
#include <bits/shared_ptr.h>
#include <array>

// Forward Declaration mandatory because of the circular dependency between Tasks, Sessions and Generators
namespace darwin {
    class ASession;
    
    /// Definition of a session's self-managing pointer.
    ///
    /// \typedef session_ptr_t
    typedef std::shared_ptr<ASession> session_ptr_t;

    /// Definition of a UDP buffer
    ///
    /// \typedef udp_buffer_t 
    typedef std::array<char, 65536> udp_buffer_t;
}