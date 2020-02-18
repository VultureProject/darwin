set(ANOMALY_NAME darwin_anomaly)

#######################
# FILTER DEPENDENCIES #
#######################

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
    ${ANOMALY_NAME}
    ${DARWIN_SOURCES}
    samples/fanomaly/AnomalyTask.cpp samples/fanomaly/AnomalyTask.hpp
    samples/fanomaly/Generator.cpp samples/fanomaly/Generator.hpp
)

target_link_libraries(
    ${ANOMALY_NAME}
    ${DARWIN_LIBRARIES}
    lapacke
    openblas
    ${ARMADILLO_LIBRARY}
    ${MLPACK_LIBRARY}
)

target_include_directories(${ANOMALY_NAME} PUBLIC ${ARMADILLO_INCLUDE_DIRS})
target_include_directories(${ANOMALY_NAME} PUBLIC ${MLPACK_INCLUDE_DIRS})
target_include_directories(${ANOMALY_NAME} PUBLIC samples/fanomaly/)