set(LOGS_NAME darwin_logs)

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
    ${LOGS_NAME}
    ${DARWIN_SOURCES}
    samples/flogs/LogsTask.cpp samples/flogs/LogsTask.hpp
    samples/flogs/Generator.cpp samples/flogs/Generator.hpp
    toolkit/RedisManager.cpp toolkit/RedisManager.hpp
    toolkit/FileManager.cpp toolkit/FileManager.hpp
)

target_link_libraries(
    ${LOGS_NAME}
    pthread
    boost_system
    hiredis
)

target_include_directories(${LOGS_NAME} PUBLIC ${HIREDIS_INCLUDE_DIRS})
target_include_directories(${LOGS_NAME} PUBLIC samples/flogs/)
