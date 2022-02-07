set(PYTHON_NAME darwin_python)

#######################
# FILTER DEPENDENCIES #
#######################

find_package(Python3 REQUIRED COMPONENTS Development)

include_directories(SYSTEM ${Python3_INCLUDE_DIRS})
link_directories(${Python3_LIBRARY_DIRS})

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
    ${Python3_LIBRARIES}
    # DL libs are used for loading shared objects
    ${CMAKE_DL_LIBS}
)

target_include_directories(${PYTHON_NAME} PUBLIC samples/fpython/)
