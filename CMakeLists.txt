cmake_minimum_required(VERSION 3.7.2)

project(darwin)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 17)

if (DEBUG)
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -g -ggdb3")
else ()
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -o2")
endif ()

if (COVERAGE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

if (NOT DEFINED FILTER)
    set(
        FILTERS
        HOSTLOOKUP
        SESSION
        DGA
        CONNECTION
        TANOMALY
        ANOMALY
        CONTENT_INSPECTION
        BUFFER
        YARA
    )
else (NOT DEFINED FILTER)
    set(
            FILTERS
            ${FILTER}
    )
endif (NOT DEFINED FILTER)


#####################
# CORE DEPENDENCIES #
#####################

set(
    ENV{PKG_CONFIG_PATH}
    "/usr/local/lib/pkgconfig/;/usr/local/libdata/pkgconfig/"
)
find_package(PkgConfig)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost
	COMPONENTS
	  system
	REQUIRED)

find_package(Hiredis REQUIRED)

set(
    DARWIN_LIBRARIES
    Threads::Threads
    Hiredis::Hiredis
    ${Boost_LIBRARIES}
)

###################
#  CORE INCLUDES  #
###################

include_directories(
        toolkit
        toolkit/thread-pool-cpp/include/
        samples/base
        samples/
	${HIREDIS_INCLUDE_DIRS}
	${Boost_INCLUDE_DIRS}
)


####################
#   CORE SOURCES   #
####################

set(
        DARWIN_SOURCES
        samples/protocol.h
        samples/base/main.cpp
        samples/config.hpp samples/config.cpp
        samples/base/Stats.cpp samples/base/Stats.hpp
        samples/base/Core.cpp samples/base/Core.hpp
        samples/base/Logger.cpp samples/base/Logger.hpp
        samples/base/Monitor.cpp samples/base/Monitor.hpp
        samples/base/AGenerator.cpp samples/base/AGenerator.hpp
        samples/base/ThreadGroup.cpp samples/base/ThreadGroup.hpp
        samples/base/AlertManager.cpp samples/base/AlertManager.hpp

        samples/base/DarwinPacket.cpp samples/base/DarwinPacket.hpp
        samples/base/ANextFilterConnector.cpp samples/base/ANextFilterConnector.hpp
        samples/base/UnixNextFilterConnector.cpp samples/base/UnixNextFilterConnector.hpp
        samples/base/TcpNextFilterConnector.cpp samples/base/TcpNextFilterConnector.hpp

        samples/base/AServer.cpp samples/base/AServer.hpp
        samples/base/UnixServer.cpp samples/base/UnixServer.hpp
        samples/base/TcpServer.cpp samples/base/TcpServer.hpp
        samples/base/Manager.cpp samples/base/Manager.hpp
        samples/base/ASession.cpp samples/base/ASession.hpp
        samples/base/UnixSession.cpp samples/base/UnixSession.hpp
        samples/base/TcpSession.cpp samples/base/TcpSession.hpp
        samples/base/ATask.cpp samples/base/ATask.hpp

        toolkit/Network.cpp toolkit/Network.hpp
        toolkit/Validators.cpp toolkit/Validators.hpp
        toolkit/Time.cpp toolkit/Time.hpp
        toolkit/Files.cpp toolkit/Files.hpp
        toolkit/RedisManager.cpp toolkit/RedisManager.hpp
        toolkit/FileManager.cpp toolkit/FileManager.hpp
        toolkit/StringUtils.cpp toolkit/StringUtils.hpp
        toolkit/Uuid.cpp toolkit/Uuid.hpp
)


###########################
#   Filters CMake Calls   #
###########################

message("Generating CMake files for the filters: ${FILTERS}")

if("CONNECTION" IN_LIST FILTERS)
include(fconnection)
endif()

if("SESSION" IN_LIST FILTERS)
include(fsession)
endif()

if("HOSTLOOKUP" IN_LIST FILTERS)
include(fhostlookup)
endif()

if("DGA" IN_LIST FILTERS)
include(fdga)
endif()

if("TANOMALY" IN_LIST FILTERS)
include(ftanomaly)
endif()

if("ANOMALY" IN_LIST FILTERS)
include(fanomaly)
endif()

if("CONTENT_INSPECTION" IN_LIST FILTERS)
include(finspection)
endif()

if("TEST" IN_LIST FILTERS)
include(ftest)
endif()

if("SOFA" IN_LIST FILTERS)
include(fsofa)
endif()

if("BUFFER" IN_LIST FILTERS)
include(fbuffer)
endif()

if("YARA" IN_LIST FILTERS)
include(fyara)
endif()

if("VAST" IN_LIST FILTERS)
include(fvast)
endif()

if("VAML" IN_LIST FILTERS)
include(fvaml)
endif()
