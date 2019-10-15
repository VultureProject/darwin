cmake_minimum_required(VERSION 3.7.2)

project(darwin_anomaly)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 17)

if (DEBUG)
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra -g -ggdb3")
else ()
    SET(CMAKE_CXX_FLAGS "-W -Wall -Wextra")
endif ()

message("Flags: ${CMAKE_CXX_FLAGS}")



#######################
# FILTER DEPENDENCIES #
#######################

set(
    ENV{PKG_CONFIG_PATH}
    "/usr/local/lib/pkgconfig/;/usr/local/libdata/pkgconfig/"
)
find_package(PkgConfig)

include_directories(/usr/local/include)

pkg_check_modules(HIREDIS REQUIRED hiredis)
include_directories(${HIREDIS_INCLUDE_DIRS})
link_directories(${HIREDIS_LIBRARY_DIRS})
find_library(HIREDIS_LIBRARY hiredis PATHS ${HIREDIS_LIBRARY_DIRS})

include_directories(${ARMADILLO_INCLUDE_DIRS})
find_library(ARMADILLO_LIBRARY armadillo PATHS ${ARMADILLO_LIBRARY_DIRS})

include_directories(${MLPACK_INCLUDE_DIRS})
find_library(MLPACK_LIBRARY mlpack PATHS ${MLPACK_LIBRARY_DIRS})

if(NOT ARMADILLO_LIBRARY)
message(WARNING "Did not find lib armadillo, will not be able to compile filters anomaly/tanomaly")
endif()

if(NOT MLPACK_LIBRARY)
message(WARNING "Did not find lib mlpack, will not be able to compile filters anomaly/tanomaly")
endif()


####################
#    FILTER CONF   #
####################

set(EXEC_NAME darwin_anomaly)

set(
    FILTER_LIBS
    hiredis
    lapack
    blas
    ${ARMADILLO_LIBRARY}
    ${MLPACK_LIBRARY}
)


####################
#  FILTER SOURCES  #
####################

set(
        FILTER_SOURCES
        AnomalyTask.cpp AnomalyTask.hpp
        Generator.cpp Generator.hpp
)


#######################
#   FILTER INCLUDES   #
#######################

include_directories(
    . # Needed for the Generator.hpp
    ${ARMADILLO_INCLUDE_DIRS}
    ${MLPACK_INCLUDE_DIRS}
)


#####################
# CORE DEPENDENCIES #
#####################

find_package(Threads)
find_package(Boost)

include_directories(${ARMADILLO_INCLUDE_DIRS})
find_library(ARMADILLO_LIBRARY armadillo PATHS ${ARMADILLO_LIBRARY_DIRS})

include_directories(${MLPACK_INCLUDE_DIRS})
find_library(MLPACK_LIBRARY mlpack PATHS ${MLPACK_LIBRARY_DIRS})

if(NOT ARMADILLO_LIBRARY)
message(WARNING "Did not find lib armadillo, will not be able to compile filters anomaly/tanomaly")
endif()

if(NOT MLPACK_LIBRARY)
message(WARNING "Did not find lib mlpack, will not be able to compile filters anomaly/tanomaly")
endif()

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
        ../../toolkit/RedisManager.cpp ../../toolkit/RedisManager.hpp
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