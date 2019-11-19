/// \file     main.cpp
/// \authors  hsoszynski
/// \version  1.0
/// \date     16/04/18
/// \license  GPLv3
/// \brief    Copyright (c) 2018 Advens. All rights reserved.

#include <csignal>
#include <cstdlib>
#include "Core.hpp"
#include "Logger.hpp"

void rotateLogsHandler(int signum __attribute__((unused))) {
    darwin::logger::Logger& log = darwin::logger::Logger::instance();
    log.log(darwin::logger::Info, "Rotating logs...");

    log.RotateLogs();
}

int main(int ac, char**av) {
    signal(SIGUSR1, rotateLogsHandler);
    signal(SIGHUP, rotateLogsHandler);

    DARWIN_LOGGER;

    darwin::Core& core = darwin::Core::instance();

    if (!core.Configure(ac, av))
        return 1;
    DARWIN_LOG_INFO("Configured");

    if (core.daemon) {
        daemon(1, 0);
    }

    if (!core.WritePID())
        return 1;

    DARWIN_LOG_INFO("Starting...");
    auto ret = core.run();

    core.ClearPID();

    return ret;
}