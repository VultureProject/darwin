set(DGA_NAME darwin_dga)

#######################
# FILTER DEPENDENCIES #
#######################

find_package(Tensorflow REQUIRED)
find_package(Faup REQUIRED)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${DGA_NAME}
    ${DARWIN_SOURCES}
    samples/fdga/DGATask.cpp samples/fdga/DGATask.hpp
    samples/fdga/Generator.cpp samples/fdga/Generator.hpp
)

target_link_libraries(
    ${DGA_NAME}
    ${DARWIN_LIBRARIES}
    ${TENSORFLOW_LIBRARIES}
    ${FAUP_LIBRARIES}
)

target_include_directories(${DGA_NAME} PUBLIC ${TENSORFLOW_INCLUDE_DIRS})
target_include_directories(${DGA_NAME} PUBLIC ${FAUP_INCLUDE_DIRS})
target_include_directories(${DGA_NAME} PUBLIC samples/fdga/)