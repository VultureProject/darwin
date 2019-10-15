cmake_minimum_required(VERSION 3.7.2)

project(darwin_content_inspection)

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

set(EXEC_NAME darwin_content_inspection)

set(
    FILTER_LIBS
    yara
)


####################
#  FILTER SOURCES  #
####################

set(
        FILTER_SOURCES
        ContentInspectionTask.cpp ContentInspectionTask.hpp
        Generator.cpp Generator.hpp
        data_pool.cpp data_pool.hpp
        file_utils.cpp file_utils.hpp
        hash_utils.cpp hash_utils.hpp
        rand_utils.cpp rand_utils.hpp
        flow.cpp flow.hpp
        packets.cpp packet-utils.hpp
        extract_impcap.cpp extract_impcap.hpp
        stream_buffer.cpp stream_buffer.hpp
        tcp_sessions.cpp tcp_sessions.hpp
        yara_utils.cpp yara_utils.hpp
        packet-utils.hpp
)


#######################
# FILTER DEPENDENCIES #
#######################

link_directories(
    ${LIBYARA_LIBRARY_DIRS}
)



#######################
#   FILTER INCLUDES   #
#######################

include_directories(
    . # Needed for the Generator.hpp
    ${LIBYARA_INCLUDE_DIRS}
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