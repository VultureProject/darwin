cmake_minimum_required(VERSION 3.7.2)

project(darwin)

set(CMAKE_VERBOSE_MAKEFILE on)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ../../)

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
message("Generating CMake files for the filters: ${FILTERS}")

###########################
#   Filters CMake Calls   #
###########################

if(LOGS IN_LIST FILTERS)
add_subdirectory(samples/flogs)
endif()

if(END IN_LIST FILTERS)
add_subdirectory(samples/fend)
endif()

if(CONNECTION IN_LIST FILTERS)
add_subdirectory(samples/fconnection)
endif()

if(SESSION IN_LIST FILTERS)
add_subdirectory(samples/fsession)
endif()

if(HOSTLOOKUP IN_LIST FILTERS)
add_subdirectory(samples/fhostlookup)
endif()

if(DGA IN_LIST FILTERS)
add_subdirectory(samples/fdga)
endif()

if(USER_AGENT IN_LIST FILTERS)
add_subdirectory(samples/fuseragent)
endif()

if(TANOMALY IN_LIST FILTERS)
add_subdirectory(samples/ftanomaly)
endif()

if(ANOMALY IN_LIST FILTERS)
add_subdirectory(samples/fanomaly)
endif()

if(REPUTATION IN_LIST FILTERS)
add_subdirectory(samples/freputation)
endif()

if(CONTENT_INSPECTION IN_LIST FILTERS)
add_subdirectory(samples/finspection)
endif()