set(SOFA_NAME darwin_sofa)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${SOFA_NAME}
    ${DARWIN_SOURCES}
    samples/fsofa/Generator.cpp samples/fsofa/Generator.hpp
    samples/fsofa/SofaTask.cpp samples/fsofa/SofaTask.hpp
    toolkit/PythonUtils.cpp toolkit/PythonUtils.hpp
)

target_link_libraries(
    ${SOFA_NAME}
    ${DARWIN_LIBRARIES}
    python3.7m
    boost_filesystem
)

target_include_directories(${SOFA_NAME} PUBLIC /usr/local/include/python3.7m/)
target_include_directories(${SOFA_NAME} PUBLIC samples/fsofa/)