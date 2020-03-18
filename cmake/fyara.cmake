set(YARA_NAME darwin_yara)

#######################
# FILTER DEPENDENCIES #
#######################

link_directories(
    ${LIBYARA_LIBRARY_DIRS}
)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${YARA_NAME}
    ${DARWIN_SOURCES}
    samples/fyarascan/Generator.cpp samples/fyarascan/Generator.hpp
    samples/fyarascan/YaraScanTask.cpp samples/fyarascan/YaraScanTask.hpp
    toolkit/Yara.cpp toolkit/Yara.hpp
)

target_link_libraries(
    ${YARA_NAME}
    ${DARWIN_LIBRARIES}
    yara
)

target_include_directories(${YARA_NAME} PUBLIC ${LIBYARA_INCLUDE_DIRS})
target_include_directories(${YARA_NAME} PUBLIC samples/fyarascan/)