set(DGA_NAME darwin_dga)

#######################
# FILTER DEPENDENCIES #
#######################

# This will deactivate XNNPACK by default as it is uncompatible with FreeBSD
set(TFLITE_ENABLE_XNNPACK OFF CACHE BOOL "Deactivate XNNPACK for the build (incompatible with freebsd)")

set(TENSORFLOW_SOURCE_DIR "" CACHE PATH
  "Directory that contains the TensorFlow project"
)
if(NOT TENSORFLOW_SOURCE_DIR)
  get_filename_component(TENSORFLOW_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../tensorflow"
    ABSOLUTE
  )
endif()

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