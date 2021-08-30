set(PYTHON_NAME darwin_python)

#######################
# FILTER DEPENDENCIES #
#######################

include_directories(SYSTEM /usr/include/python3.8)
link_directories(/usr/lib/python3.8/config-3.8-x86)

###################
#    EXECUTABLE   #
###################


add_executable(
    ${PYTHON_NAME}
    ${DARWIN_SOURCES}
    samples/fpython/PythonTask.cpp samples/fpython/PythonTask.hpp
    samples/fpython/Generator.cpp samples/fpython/Generator.hpp
)

target_link_libraries(
    ${PYTHON_NAME}
    ${DARWIN_LIBRARIES}
    python3.8
    crypt
    pthread
    dl
    util
    m
)

target_include_directories(${PYTHON_NAME} PUBLIC samples/fpython/)
