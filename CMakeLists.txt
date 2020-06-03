cmake_minimum_required(VERSION 3.7.2)

project(darwin)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 17)

if (DEBUG)
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -g -ggdb3")
else ()
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra")
endif ()

option(DEBUG "Compile Darwin filters with debugging capabilities" OFF)
option(GIT_SUBMODULE "Check submodules during build" ON)

include(${PROJECT_SOURCE_DIR}/cmake/catch_tests.cmake)

if(GIT_SUBMODULE)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        # Update submodules as needed
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    else()
        message(FATAL_ERROR "git not present, could not check submodules ! Please install git or disable submodules checking with -DGIT_SUBMODULE=OFF !")
    endif()
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
        LOGS
        CONTENT_INSPECTION
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

        samples/base/Server.cpp samples/base/Server.hpp
        samples/base/Manager.cpp samples/base/Manager.hpp
        samples/base/Session.cpp samples/base/Session.hpp

        toolkit/Network.cpp toolkit/Network.hpp
        toolkit/Validators.cpp toolkit/Validators.hpp
        toolkit/Time.cpp toolkit/Time.hpp
        toolkit/Files.cpp toolkit/Files.hpp
        toolkit/RedisManager.cpp toolkit/RedisManager.hpp
        toolkit/FileManager.cpp toolkit/FileManager.hpp
        toolkit/Encoders.h
)


###########################
#   Filters CMake Calls   #
###########################

###########################
#   YARA SCANNER FILTER   #
###########################

set(
        YARA_SCAN_SOURCES
        samples/fyarascan/Generator.cpp samples/fyarascan/Generator.hpp
        samples/fyarascan/YaraScanTask.cpp samples/fyarascan/YaraScanTask.hpp
        toolkit/Yara.cpp toolkit/Yara.hpp
)

set(
        YARA_SCAN_GENERATOR_DIRECTORY
        samples/fyarascan/
)

set(
        YARA_SCAN_LIBS
        yara
)

set(
        YARA_SCAN_INC
        ${LIBYARA_INCLUDE_DIRS}
)

set(
        YARA_SCAN_LIBS_DIRS
        ${LIBYARA_LIBRARY_DIRS}
)

###########################
#  !YARA SCANNER FILTER   #
###########################

message("Generating CMake files for the filters: ${FILTERS}")

if("LOGS" IN_LIST FILTERS)
include(flogs)
endif()

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

<<<<<<< HEAD
if("YARA" IN_LIST FILTERS)
=======
if(YARA IN_LIST FILTERS)
>>>>>>> b3af0a0d01130878f4263392fc372b05d208afcd
include(fyara)
endif()
