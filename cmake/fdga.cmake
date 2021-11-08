set(DGA_NAME darwin_dga)

#######################
# FILTER DEPENDENCIES #
#######################

if(NOT TENSORFLOW_SOURCE_DIR)
  set(TENSORFLOW_SOURCE_DIR "../tensorflow_src")
endif()

set(TENSORFLOW_SOURCE_DIR "" CACHE PATH
  "Directory that contains the TensorFlow project"
) 

add_subdirectory(
  "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
  "${CMAKE_CURRENT_BINARY_DIR}/tensorflow-lite" EXCLUDE_FROM_ALL)

find_package(Faup REQUIRED)

###################
#    EXECUTABLE   #
###################

add_executable(
    ${DGA_NAME}
    ${DARWIN_SOURCES}
    samples/fdga/DGATask.cpp samples/fdga/DGATask.hpp
    samples/fdga/Generator.cpp samples/fdga/Generator.hpp
    samples/fdga/TfLiteHelper.cpp samples/fdga/TfLiteHelper.hpp
)

target_link_libraries(
    ${DGA_NAME}
    ${DARWIN_LIBRARIES}
    tensorflow-lite
    ${FAUP_LIBRARIES}
)

target_include_directories(${DGA_NAME} PUBLIC ${FAUP_INCLUDE_DIRS})
target_include_directories(${DGA_NAME} PUBLIC samples/fdga/)