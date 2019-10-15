cmake_minimum_required(VERSION 3.7.2)

project(darwin_user_agent)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 17)

if (DEBUG)
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -g -ggdb3")
else ()
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra")
endif ()

message("Flags: ${CMAKE_CXX_FLAGS}")


####################
#    FILTER CONF   #
####################

set(EXEC_NAME darwin_user_agent)

set(
    FILTER_LIBS
    libtensorflow_cc.so
    libtensorflow_framework.so
    libexecinfo.so
    libfaupl.so
)


####################
#  FILTER SOURCES  #
####################

set(
        FILTER_SOURCES
        UserAgentTask.cpp UserAgentTask.hpp
        Generator.cpp Generator.hpp
)


#######################
#   FILTER INCLUDES   #
#######################

include_directories(
    . # Needed for the Generator.hpp
    /usr/local/include/contrib/eigen/
    /usr/local/include/contrib/absl/
)


#######################
# FILTER DEPENDENCIES #
#######################

find_package(faup)

link_directories(
    /usr/local/lib/tensorflow/
)


#####################
# CORE DEPENDENCIES #
#####################

find_package(Threads)
find_package(Boost)

link_directories(
    /usr/local/lib
)


####################
#   CORE SOURCES   #
####################

set(
        DARWIN_SOURCES
        ../protocol.h
        ../base/main.cpp
        ../config.hpp ../config.cpp
        ../base/Core.cpp ../base/Core.hpp
        ../base/Logger.cpp ../base/Logger.hpp
        ../base/Monitor.cpp ../base/Monitor.hpp

        ../base/Server.cpp ../base/Server.hpp
        ../base/Manager.cpp ../base/Manager.hpp
        ../base/Session.cpp ../base/Session.hpp

        ../../toolkit/Network.cpp ../../toolkit/Network.hpp
        ../../toolkit/Validators.cpp ../../toolkit/Validators.hpp
        ../../toolkit/Time.cpp ../../toolkit/Time.hpp
        ../../toolkit/Files.cpp ../../toolkit/Files.hpp
)


###################
#     INCLUDES    #
###################

include_directories(
        ../../toolkit
        ../
        ../base
        /usr/local/include
)


###################
#    EXECUTABLE   #
###################

add_executable(
    ${EXEC_NAME}
    ${DARWIN_SOURCES}
    ${FILTER_SOURCES}
)

target_link_libraries(
    ${EXEC_NAME}
    ${FILTER_LIBS}
    pthread
    boost_system
)