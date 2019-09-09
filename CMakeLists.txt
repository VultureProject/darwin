cmake_minimum_required(VERSION 3.7.2)

project(darwin)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 14)
set(ENV{PKG_CONFIG_PATH}  "/usr/local/lib/pkgconfig/;/usr/local/libdata/pkgconfig/")

find_package(PkgConfig)

if (DEBUG)
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -g -ggdb3")
else ()
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra")
endif ()

message("Flags: ${CMAKE_CXX_FLAGS}")

####################
#    CONNECTORS    #
####################

find_package(Threads)
find_package(faup)
pkg_check_modules(LIBEVENT REQUIRED libevent)
pkg_check_modules(LIBEVENT_PTHREADS REQUIRED libevent_pthreads)
link_directories(
        ${LIBEVENT_LIBRARY_DIRS}
        ${LIBEVENT_PTHREADS_LIBRARY_DIRS}
)

pkg_check_modules(HIREDIS REQUIRED hiredis)
include_directories( ${HIREDIS_INCLUDE_DIRS} )
include_directories(/usr/local/include)
link_directories(
        ${HIREDIS_LIBRARY_DIRS}
)

find_path(MLPACK_INCLUDE_DIRS
  NAMES mlpack/core.hpp mlpack/prereqs.hpp
  PATHS ./mlpack/build/include/
)

find_library(MLPACK_LIBRARY
  NAMES mlpack
  PATHS ./mlpack/build/lib ./mlpack/build/include
)

if(NOT DEFINED MLPACK_LIBRARY)
    message("Mlpack library not found")
endif(NOT DEFINED MLPACK_LIBRARY)

find_library(ARMADILLO_LIBRARY
  NAMES armadillo
  PATHS ./armadillo-code/include/lib /usr/local/lib
)

set(ARMADILLO_INCLUDE_DIRS ./armadillo-code/include/)

if(NOT DEFINED ARMADILLO_LIBRARY)
    message("Armadillo library not found")
endif(NOT DEFINED ARMADILLO_LIBRARY)

####################
#   !CONNECTORS    #
####################

set(
        DARWIN_SOURCES
        samples/protocol.h
        samples/base/main.cpp
        samples/config.hpp samples/config.cpp
        samples/base/Core.cpp samples/base/Core.hpp
        samples/base/Logger.cpp samples/base/Logger.hpp
        samples/base/Monitor.cpp samples/base/Monitor.hpp

        samples/base/Server.cpp samples/base/Server.hpp
        samples/base/Manager.cpp samples/base/Manager.hpp
        samples/base/Session.cpp samples/base/Session.hpp

        toolkit/Network.cpp toolkit/Network.hpp
        toolkit/Validators.cpp toolkit/Validators.hpp
        toolkit/RedisManager.cpp toolkit/RedisManager.hpp
        toolkit/ThreadManager.cpp toolkit/ThreadManager.hpp
        toolkit/Time.cpp toolkit/Time.hpp
)

find_package(Boost)

######################
#  USER_AGENT FILTER #
######################

set(
        USER_AGENT_SOURCES
        samples/fuseragent/UserAgentTask.cpp samples/fuseragent/UserAgentTask.hpp
        samples/fuseragent/Generator.cpp samples/fuseragent/Generator.hpp
)

set(
        USER_AGENT_GENERATOR_DIRECTORY
        samples/fuseragent/
)

set(
        USER_AGENT_LIBS
        libtensorflow_cc.so
        libtensorflow_framework.so
        libexecinfo.so
)

set(
        USER_AGENT_INC
        /usr/local/include/contrib/eigen/
        /usr/local/include/contrib/absl/
)

set(
        USER_AGENT_LIBS_DIRS
        /usr/local/lib/tensorflow/
)

######################
# !USER_AGENT FILTER #
######################

######################
#     DGA FILTER     #
######################

set(
        DGA_SOURCES
        samples/fdga/DGATask.cpp samples/fdga/DGATask.hpp
        samples/fdga/Generator.cpp samples/fdga/Generator.hpp
)

set(
        DGA_GENERATOR_DIRECTORY
        samples/fdga/
)

set(
        DGA_LIBS
        libtensorflow_cc.so
        libtensorflow_framework.so
        libexecinfo.so
        libfaupl.so
)

set(
        DGA_INC
        /usr/local/include/contrib/eigen/
        /usr/local/include/contrib/absl/
)

set(
        DGA_LIBS_DIRS
        /usr/local/lib/tensorflow/
)

######################
#    !DGA FILTER     #
######################

######################
# HOSTLOOKUP FILTER  #
######################

set(
        HOSTLOOKUP_SOURCES
        samples/fhostlookup/HostLookupTask.cpp samples/fhostlookup/HostLookupTask.hpp
        samples/fhostlookup/Generator.cpp samples/fhostlookup/Generator.hpp
)

set(
        HOSTLOOKUP_GENERATOR_DIRECTORY
        samples/fhostlookup/
)

set(
        HOSTLOOKUP_LIBS
)

set(
        HOSTLOOKUP_INC
)

set(
        HOSTLOOKUP_LIBS_DIRS
)

######################
# !HOSTLOOKUP FILTER #
######################


######################
#   SESSION FILTER   #
######################

set(
        SESSION_SOURCES
        samples/fsession/SessionTask.cpp samples/fsession/SessionTask.hpp
        samples/fsession/Generator.cpp samples/fsession/Generator.hpp
)

set(
        SESSION_GENERATOR_DIRECTORY
        samples/fsession/
)

pkg_check_modules(HIREDIS REQUIRED hiredis)

set(
        HIREDIS_LIBS
        hiredis
)

set(
        HIREDIS_INC
        ${HIREDIS_INCLUDE_DIRS}
)

set(
        HIREDIS_LIBS_DIRS
        ${HIREDIS_LIBRARY_DIRS}
)


######################
#  !SESSION FILTER   #
######################


######################
#  REPUTATION FILTER #
######################

set(
        REPUTATION_SOURCES
        samples/freputation/ReputationTask.cpp samples/freputation/ReputationTask.hpp
        samples/freputation/Generator.cpp samples/freputation/Generator.hpp
)

set(
        REPUTATION_GENERATOR_DIRECTORY
        samples/freputation/
)

pkg_check_modules(MAXMINDDB REQUIRED libmaxminddb)

set(
        REPUTATION_LIBS
        maxminddb
)

set(
        REPUTATION_INC
        ${MAXMINDDB_INCLUDE_DIRS}
)

set(
        REPUTATION_LIBS_DIRS
        ${MAXMINDDB_LIBRARY_DIRS}
)


######################
# !REPUTATION FILTER #
######################


#########################
#    DECISION FILTER    #
#########################

set(
        DECISION_SOURCES
        samples/fdecision/DecisionTask.cpp samples/fdecision/DecisionTask.hpp
        samples/fdecision/Generator.cpp samples/fdecision/Generator.hpp
)

set(
        DECISION_GENERATOR_DIRECTORY
        samples/fdecision/
)

set(
        DECISION_LIBS
)

set(
        DECISION_INC
)

set(
        DECISION_LIBS_DIRS
)

#########################
#   !DECISION FILTER    #
#########################

#########################
#   CONNECTION FILTER   #
#########################

set(
        CONNECTION_SOURCES
        samples/fconnection/ConnectionSupervisionTask.cpp samples/fconnection/ConnectionSupervisionTask.hpp
        samples/fconnection/Generator.cpp samples/fconnection/Generator.hpp
)

set(
        CONNECTION_GENERATOR_DIRECTORY
        samples/fconnection/
)

set(
        CONNECTION_LIBS
)

set(
        CONNECTION_INC
)

set(
        CONNECTION_LIBS_DIRS
)

#########################
#  !CONNECTION FILTER   #
#########################

#########################
#    TANOMALY FILTER   #
#########################

set(
        TANOMALY_SOURCES
        samples/ftanomaly/TAnomalyThreadManager.cpp samples/ftanomaly/TAnomalyThreadManager.hpp
        samples/ftanomaly/TAnomalyTask.cpp samples/ftanomaly/TAnomalyTask.hpp
        samples/ftanomaly/Generator.cpp samples/ftanomaly/Generator.hpp
)

set(
        TANOMALY_GENERATOR_DIRECTORY
        samples/ftanomaly/
)

set(
        TANOMALY_LIBS
        lapack
        blas
        ${ARMADILLO_LIBRARY}
        ${MLPACK_LIBRARY}
)

set(
        TANOMALY_INC
        ${ARMADILLO_INCLUDE_DIRS}
        ${MLPACK_INCLUDE_DIRS}
)

set(
        TANOMALY_LIBS_DIRS
        ${TANOMALY_LIBRARY_DIRS}
)

#########################
#   !TANOMALY FILTER    #
#########################

#########################
#   ANOMALY FILTER   #
#########################

set(
        ANOMALY_SOURCES
        samples/fanomaly/AnomalyTask.cpp samples/fanomaly/AnomalyTask.hpp
        samples/fanomaly/Generator.cpp samples/fanomaly/Generator.hpp
)

set(
        ANOMALY_GENERATOR_DIRECTORY
        samples/fanomaly/
)

set(
        ANOMALY_LIBS
        lapack
        blas
        ${ARMADILLO_LIBRARY}
        ${MLPACK_LIBRARY}
)

set(
        ANOMALY_INC
        ${ARMADILLO_INCLUDE_DIRS}
        ${MLPACK_INCLUDE_DIRS}
)

set(
        ANOMALY_LIBS_DIRS
        ${ANOMALY_LIBRARY_DIRS}
)

#########################
#    !ANOMALY FILTER    #
#########################

#########################
#      LOGS FILTER      #
#########################

set(
        LOGS_SOURCES
        samples/flogs/LogsTask.cpp samples/flogs/LogsTask.hpp
        samples/flogs/Generator.cpp samples/flogs/Generator.hpp
)

set(
        LOGS_GENERATOR_DIRECTORY
        samples/flogs/
)

set(
        LOGS_LIBS
)

set(
        LOGS_INC
)

set(
        LOGS_LIBS_DIRS
        ${LOGS_LIBRARY_DIRS}
)

#########################
#     !LOGS FILTER      #
#########################

#########################
#       END FILTER      #
#########################

set(
        END_SOURCES
        samples/fend/EndTask.cpp samples/fend/EndTask.hpp
        samples/fend/Generator.cpp samples/fend/Generator.hpp
)

set(
        END_GENERATOR_DIRECTORY
        samples/fend/
)

set(
        END_LIBS
)

set(
        END_INC
)

set(
        END_LIBS_DIRS
        ${END_LIBRARY_DIRS}
)

#########################
#      !END FILTER      #
#########################



#########################
# PYTHON_EXAMPLE FILTER #
#########################

set(
        PYTHON_EXAMPLE_SOURCES
        samples/fpythonexample/PythonExampleTask.cpp samples/fpythonexample/PythonExampleTask.hpp
        samples/fpythonexample/Generator.cpp samples/fpythonexample/Generator.hpp

        toolkit/PythonUtils.cpp toolkit/PythonUtils.hpp
)

set(
        PYTHON_EXAMPLE_GENERATOR_DIRECTORY
        samples/fpythonexample/
)

set(
        PYTHON_EXAMPLE_LIBS
        python3.6m
        boost_filesystem
)

set(
        PYTHON_EXAMPLE_INC
        /usr/local/include/python3.6m/
)

set(
        PYTHON_EXAMPLE_LIBS_DIRS
)

#########################
#!PYTHON_EXAMPLE FILTER #
#########################


#################################
#   CONTENT INSPECTION FILTER   #
#################################

set(
        CONTENT_INSPECTION_SOURCES
        samples/finspection/ContentInspectionTask.cpp samples/finspection/ContentInspectionTask.hpp
        samples/finspection/Generator.cpp samples/finspection/Generator.hpp
        samples/finspection/data_pool.cpp samples/finspection/data_pool.hpp
        samples/finspection/file_utils.cpp samples/finspection/file_utils.hpp
        samples/finspection/hash_utils.cpp samples/finspection/hash_utils.hpp
        samples/finspection/rand_utils.cpp samples/finspection/rand_utils.hpp
        samples/finspection/flow.cpp samples/finspection/flow.hpp
        samples/finspection/packets.cpp samples/finspection/packet-utils.hpp
        samples/finspection/extract_impcap.cpp samples/finspection/extract_impcap.hpp
        samples/finspection/stream_buffer.cpp samples/finspection/stream_buffer.hpp
        samples/finspection/tcp_sessions.cpp samples/finspection/tcp_sessions.hpp
        samples/finspection/yara_utils.cpp samples/finspection/yara_utils.hpp
        samples/finspection/packet-utils.hpp
)

set(
        CONTENT_INSPECTION_GENERATOR_DIRECTORY
        samples/finspection/
)

set(
        CONTENT_INSPECTION_LIBS
        yara
)

set(
        CONTENT_INSPECTION_INC
        ${LIBYARA_INCLUDE_DIRS}
)

set(
        CONTENT_INSPECTION_LIBS_DIRS
        ${LIBYARA_LIBRARY_DIRS}
)

#################################
#  !CONTENT INSPECTION FILTER   #
#################################

if (NOT DEFINED FILTER)
    set(
        FILTERS
        USER_AGENT
        HOSTLOOKUP
        SESSION
        REPUTATION
        DGA
        CONNECTION
        TANOMALY
        ANOMALY
        LOGS
        END
        CONTENT_INSPECTION
    )
else (NOT DEFINED FILTER)
    set(
            FILTERS
            ${FILTER}
    )
endif (NOT DEFINED FILTER)
message("Generating CMake files for the filters: ${FILTERS}")

foreach(FILTER ${FILTERS})

    message("Creating lib: ${FILTER}")

    string(TOLOWER "${FILTER}" LIB_NAME)

    include_directories(
            toolkit
            filters
            samples/
            samples/base
            samples/base/network
            samples/base/thread
            ${${FILTER}_INC}
    )

    add_library(
            ${LIB_NAME}
            STATIC
            ${${FILTER}_SOURCES}
    )

    message("Lib created")

endforeach()

foreach(FILTER ${FILTERS})

    message("Creating filter: ${FILTER}")

    string(TOLOWER "darwin_${FILTER}" EXEC_NAME)
    string(TOLOWER "${FILTER}" LIB_NAME)

    link_directories(
            ${${FILTER}_LIBS_DIRS}
    )

    add_executable(
            ${EXEC_NAME}
            ${DARWIN_SOURCES}
    )
    target_include_directories(
            ${EXEC_NAME}
            PRIVATE
            ${${FILTER}_GENERATOR_DIRECTORY}
    )
    target_link_libraries(
            ${EXEC_NAME}
            ${LIB_NAME}
            ${${FILTER}_LIBS}
            pthread
            boost_system
            hiredis
    )

    message("Executable name: ${EXEC_NAME}")

endforeach()
