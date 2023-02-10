set(PCR_NAME darwin_pcr)

#######################
# FILTER DEPENDENCIES #
#######################

# None

###################
#    EXECUTABLE   #
###################

add_executable(
    ${PCR_NAME}
    ${DARWIN_SOURCES}
    samples/fpcr/PCRTask.cpp samples/fpcr/PCRTask.hpp
    samples/fpcr/Generator.cpp samples/fpcr/Generator.hpp
)

target_link_libraries(
    ${PCR_NAME}
    ${DARWIN_LIBRARIES}
)

target_include_directories(${PCR_NAME} PUBLIC samples/fpcr/)