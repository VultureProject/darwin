set(SESSION_NAME darwin_session)

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
    ${SESSION_NAME}
    ${DARWIN_SOURCES}
    samples/fsession/SessionTask.cpp samples/fsession/SessionTask.hpp
    samples/fsession/Generator.cpp samples/fsession/Generator.hpp
    toolkit/RedisManager.cpp toolkit/RedisManager.hpp
)

target_link_libraries(
    ${SESSION_NAME}
    pthread
    boost_system
    hiredis
)

target_include_directories(${SESSION_NAME} PUBLIC ${HIREDIS_INCLUDE_DIRS})
target_include_directories(${SESSION_NAME} PUBLIC samples/fsession/)
