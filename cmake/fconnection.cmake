set(CONNECTION_NAME darwin_connection)

#######################
# FILTER DEPENDENCIES #
#######################

set(
    ENV{PKG_CONFIG_PATH}
    "/usr/local/lib/pkgconfig/;/usr/local/libdata/pkgconfig/"
)
find_package(PkgConfig)

pkg_check_modules(HIREDIS REQUIRED hiredis)
link_directories(${HIREDIS_LIBRARY_DIRS})
find_library(HIREDIS_LIBRARY hiredis PATHS ${HIREDIS_LIBRARY_DIRS})


###################
#    EXECUTABLE   #
###################

add_executable(
    ${CONNECTION_NAME}
    ${DARWIN_SOURCES}
    samples/fconnection/ConnectionSupervisionTask.cpp samples/fconnection/ConnectionSupervisionTask.hpp
    samples/fconnection/Generator.cpp samples/fconnection/Generator.hpp
    toolkit/RedisManager.cpp toolkit/RedisManager.hpp
)

target_link_libraries(
    ${CONNECTION_NAME}
    pthread
    boost_system
    hiredis
)

target_include_directories(${CONNECTION_NAME} PUBLIC ${HIREDIS_INCLUDE_DIRS})
target_include_directories(${CONNECTION_NAME} PUBLIC samples/fconnection/)
