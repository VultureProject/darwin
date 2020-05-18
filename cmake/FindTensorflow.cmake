# Find tensorflow cpp libraries and includes
# This module defines
#  TENSORFLOW_LIBRARIES, the libraries needed to use tensorflow
#  TENSORFLOW_INCLUDE_DIRS, the folders containing tensorflow development files
#  TENSORFLOW_FOUND, whether tensorflow was found on system

find_package(TensorflowCC QUIET)

if(TensorflowCC_FOUND)
  message("found TensorflowCC taget")
  set(TENSORFLOW_LIBRARIES "${TENSORFLOW_LIBRARIES}" "TensorflowCC::Static")
else()
  message("didn't find TensorflowCC target, linking tensorflow manually")

  find_library(
    TF_CC_LIBRARY
    NAMES libtensorflow_cc.so
    PATH_SUFFIXES tensorflow/)
  find_library(
    TF_FRAMEWORK_LIBRARY
    NAMES libtensorflow_framework.so
    PATH_SUFFIXES tensorflow/)

  find_path(
    PROTOBUF_SOURCE
    NAMES google/protobuf/stubs/common.h
    PATH_SUFFIXES contrib/protobuf/src/)
  find_path(
    EIGEN_SOURCE
    NAMES unsupported/Eigen/CXX11/Tensor
    PATH_SUFFIXES contrib/eigen)
  find_path(
      ABSL_SOURCE
      NAMES absl/strings/string_view.h
      PATH_SUFFIXES contrib/absl)

  set(TENSORFLOW_LIBRARIES ${TENSORFLOW_LIBRARIES} ${TF_CC_LIBRARY})
  set(TENSORFLOW_LIBRARIES ${TENSORFLOW_LIBRARIES} ${TF_FRAMEWORK_LIBRARY})
  set(TENSORFLOW_INCLUDE_DIRS ${TENSORFLOW_INCLUDE_DIRS} ${PROTOBUF_SOURCE})
  set(TENSORFLOW_INCLUDE_DIRS ${TENSORFLOW_INCLUDE_DIRS} ${EIGEN_SOURCE})
  set(TENSORFLOW_INCLUDE_DIRS ${TENSORFLOW_INCLUDE_DIRS} ${ABSL_SOURCE})

  include (FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Tensorflow
    REQUIRED_VARS TF_CC_LIBRARY TF_FRAMEWORK_LIBRARY)

  mark_as_advanced(TENSORFLOW_LIBRARIES TENSORFLOW_INCLUDE_DIRS)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  find_library(EXECINFO_LIBRARY
          NAMES execinfo
          PATHS /usr/local/lib/ /usr/lib/)
  if(EXECINFO_LIBRARY)
    message("libexecinfo found, adding to included libraries")
    set(TENSORFLOW_LIBRARIES ${TENSORFLOW_LIBRARIES} ${EXECINFO_LIBRARY})
  endif()
endif()
