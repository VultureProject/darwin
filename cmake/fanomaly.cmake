set(ANOMALY_NAME darwin_anomaly)

#######################
# FILTER DEPENDENCIES #
#######################

find_package(Armadillo 9.400.0 REQUIRED)

find_package(Mlpack 3.0.1 REQUIRED)

set(Boost_STATIC_LIBS ON)
# MLPACK Boost dependencies
find_package(Boost
	COMPONENTS
        program_options
        unit_test_framework
        serialization
	REQUIRED)

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
    ${ARMADILLO_LIBRARIES}
    ${MLPACK_LIBRARIES}
    Boost::program_options
    Boost::unit_test_framework
    Boost::serialization
)

target_include_directories(${ANOMALY_NAME} PUBLIC ${ARMADILLO_INCLUDE_DIRS})
target_include_directories(${ANOMALY_NAME} PUBLIC ${MLPACK_INCLUDE_DIRS})
target_include_directories(${ANOMALY_NAME} PUBLIC samples/fanomaly/)
