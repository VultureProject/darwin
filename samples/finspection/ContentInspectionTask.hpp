/// \file     ContentInspection.hpp
/// \authors  jjourdin
/// \version  1.0
/// \date     10/09/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#pragma once

#include <string>

#include "../../toolkit/lru_cache.hpp"
#include "../../toolkit/rapidjson/stringbuffer.h"
#include "../../toolkit/rapidjson/writer.h"
#include "protocol.h"
#include "Session.hpp"

#include "tcp_sessions.hpp"
#include "stream_buffer.hpp"
#include "flow.hpp"
#include "yara_utils.hpp"
#include "extract_impcap.hpp"

#define DARWIN_FILTER_CONTENT_INSPECTION 0x79617261

typedef struct Configurations_t {
    StreamsCnf *streamsCnf;
    FlowCnf *flowCnf;
    YaraCnf *yaraCnf;
}Configurations;

// To create a usable task method you MUST inherit from darwin::thread::Task publicly.
// The code bellow show all what's necessary to have a working task.
// For more information about Tasks, please refer to the class definition.

class ContentInspectionTask : public darwin::Session {
public:
    explicit ContentInspectionTask(boost::asio::local::stream_protocol::socket& socket,
                            darwin::Manager& manager,
                            std::shared_ptr<boost::compute::detail::lru_cache<xxh::hash64_t, unsigned int>> cache,
                            Configurations configurations);

    ~ContentInspectionTask() override = default;

public:
    // You need to override the functor to compile and be executed by the thread
    void operator()() override;

protected:
    /// Return filter code
    long GetFilterCode() noexcept override;

private:
    /// According to the header response,
    /// init the following Darwin workflow
    void Workflow();

    /// Parse the body received.
    bool ParseBody() override;

private:
    Configurations _configurations;
    std::vector<Packet *> _packetList;
};
