set(TANOMALY_NAME darwin_tanomaly)

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

find_library(ARMADILLO_LIBRARY armadillo PATHS ${ARMADILLO_LIBRARY_DIRS})

find_library(MLPACK_LIBRARY mlpack PATHS ${MLPACK_LIBRARY_DIRS})

if(NOT ARMADILLO_LIBRARY)
message(WARNING "Did not find lib armadillo, will not be able to compile filters anomaly/tanomaly")
endif()

if(NOT MLPACK_LIBRARY)
message(WARNING "Did not find lib mlpack, will not be able to compile filters anomaly/tanomaly")
endif()


###################
#    EXECUTABLE   #
###################

add_executable(
    ${TANOMALY_NAME}
    ${DARWIN_SOURCES}
    samples/ftanomaly/TAnomalyTask.cpp samples/ftanomaly/TAnomalyTask.hpp
    samples/ftanomaly/TAnomalyThreadManager.cpp samples/ftanomaly/TAnomalyThreadManager.hpp
    samples/ftanomaly/Generator.cpp samples/ftanomaly/Generator.hpp
    toolkit/RedisManager.cpp toolkit/RedisManager.hpp
    toolkit/FileManager.cpp toolkit/FileManager.hpp
    toolkit/ThreadManager.cpp toolkit/ThreadManager.hpp
)

target_link_libraries(
    ${TANOMALY_NAME}
    pthread
    boost_system
    hiredis
    lapack
    blas
    ${ARMADILLO_LIBRARY}
    ${MLPACK_LIBRARY}
)

target_include_directories(${TANOMALY_NAME} PUBLIC ${HIREDIS_INCLUDE_DIRS})
target_include_directories(${TANOMALY_NAME} PUBLIC ${ARMADILLO_INCLUDE_DIRS})
target_include_directories(${TANOMALY_NAME} PUBLIC ${MLPACK_INCLUDE_DIRS})
target_include_directories(${TANOMALY_NAME} PUBLIC samples/ftanomaly/)