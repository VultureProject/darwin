set(TANOMALY_NAME darwin_tanomaly)

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
    ${TANOMALY_NAME}
    ${DARWIN_SOURCES}
    samples/ftanomaly/TAnomalyTask.cpp samples/ftanomaly/TAnomalyTask.hpp
    samples/ftanomaly/TAnomalyThreadManager.cpp samples/ftanomaly/TAnomalyThreadManager.hpp
    samples/ftanomaly/Generator.cpp samples/ftanomaly/Generator.hpp
    toolkit/ThreadManager.cpp toolkit/ThreadManager.hpp
)

target_link_libraries(
    ${TANOMALY_NAME}
    ${DARWIN_LIBRARIES}
    ${ARMADILLO_LIBRARIES}
    ${MLPACK_LIBRARIES}
    Boost::program_options
    Boost::unit_test_framework
    Boost::serialization)

target_include_directories(${TANOMALY_NAME} PUBLIC ${ARMADILLO_INCLUDE_DIRS})
target_include_directories(${TANOMALY_NAME} PUBLIC ${MLPACK_INCLUDE_DIRS})
target_include_directories(${TANOMALY_NAME} PUBLIC samples/ftanomaly/)
