set(NSIGMA_NAME darwin_nsigma)

#######################
# FILTER DEPENDENCIES #
#######################

find_package(Armadillo 9.400.0 REQUIRED)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${NSIGMA_NAME}
    ${DARWIN_SOURCES}
    samples/fnsigma/NSigmaTask.cpp samples/fnsigma/NSigmaTask.hpp
    samples/fnsigma/Generator.cpp samples/fnsigma/Generator.hpp
)

target_link_libraries(
    ${NSIGMA_NAME}
    ${ARMADILLO_LIBRARIES}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${NSIGMA_NAME} PUBLIC ${ARMADILLO_INCLUDE_DIRS})
target_include_directories(${NSIGMA_NAME} PUBLIC samples/fnsigma/)