# - Find a MLPACK library with includes
# This module defines
#  MLPACK_LIBRARIES, the libraries needed to use MLPACK.
#  MLPACK_INCLUDE_DIRS, where to find MLPACK header files.
#  MLPACK_FOUND, If false, do not try to use MLPACK.
#  MLPACK_VERSION_MAJOR, the major version number.
#  MLPACK_VERSION_MINOR, the minor version number.
#  MLPACK_VERSION_PATCH, the patch version number.
#  MLPACK_VERSION_STRING, a string representing the full version.
#  MLPACK_USE_OPENMP, whether MLPACK was compiled using OpenMP.

set(MLPACK_NAMES ${MLPACK_NAMES} libmlpack.a mlpack)

set(MLPACK_VERSION_MAJOR O)
set(MLPACK_VERSION_MINOR O)
set(MLPACK_VERSION_PATCH O)
set(MLPACK_USE_OPENMP "OFF")
set(MLPACK_FOUND "NO")

find_library(MLPACK_LIBRARY
  NAMES ${MLPACK_NAMES}
  HINTS ${MLPACK_ROOT}
  PATH_SUFFIXES /lib64 /lib)

find_path(MLPACK_INCLUDE_DIR
  NAMES mlpack/
  HINTS ${MLPACK_ROOT}
  PATH_SUFFIXES include/)


if (MLPACK_INCLUDE_DIR)

  if(EXISTS "${MLPACK_INCLUDE_DIR}/mlpack/core/util/version.hpp")

    # Read and parse MLPACK's version header
    file(STRINGS "${MLPACK_INCLUDE_DIR}/mlpack/core/util/version.hpp" _MLPACK_VERSION_CONTENTS REGEX "#define MLPACK_VERSION_[A-Z]+ ")
    string(REGEX REPLACE ".*#define MLPACK_VERSION_MAJOR ([0-9]+).*" "\\1" MLPACK_VERSION_MAJOR "${_MLPACK_VERSION_CONTENTS}")
    string(REGEX REPLACE ".*#define MLPACK_VERSION_MINOR ([0-9]+).*" "\\1" MLPACK_VERSION_MINOR "${_MLPACK_VERSION_CONTENTS}")
    string(REGEX REPLACE ".*#define MLPACK_VERSION_PATCH ([0-9]+).*" "\\1" MLPACK_VERSION_PATCH "${_MLPACK_VERSION_CONTENTS}")

    set(MLPACK_VERSION_STRING "${MLPACK_VERSION_MAJOR}.${MLPACK_VERSION_MINOR}.${MLPACK_VERSION_PATCH}")
  endif()

  # Read and parse MLPACK's armadillo configuration
  if(EXISTS "${MLPACK_INCLUDE_DIR}/mlpack/core/util/arma_config.hpp")
    file(STRINGS "${MLPACK_INCLUDE_DIR}/mlpack/core/util/arma_config.hpp" _MLPACK_ARMA_CONF_CONTENTS REGEX "#define MLPACK_ARMA_(DONT_)?USE_OPENMP")

    # If MLPACK was compiled with OpenMP support, try to find it
    if(_MLPACK_ARMA_CONF_CONTENTS MATCHES ".*#define MLPACK_ARMA_USE_OPENMP.*")
      find_package(OpenMP REQUIRED)
      if (OPENMP_FOUND)
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
      endif()
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Mlpack
  REQUIRED_VARS MLPACK_LIBRARY MLPACK_INCLUDE_DIR
  VERSION_VAR MLPACK_VERSION_STRING)

if (MLPACK_FOUND)
  set(MLPACK_INCLUDE_DIRS ${MLPACK_INCLUDE_DIR})
  set(MLPACK_LIBRARIES ${MLPACK_LIBRARY})
endif()

unset(_MLPACK_VERSION_CONTENTS)
unset(_MLPACK_ARMA_CONF_CONTENTS)
unset(_MLPACK_OPENMP)

mark_as_advanced(MLPACK_LIBRARY MLPACK_INCLUDE_DIR)
