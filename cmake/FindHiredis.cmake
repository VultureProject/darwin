# Find hiredis library and includes
# This module defines
#  HIREDIS_LIBRARIES, the libraries needed to use hiredis
#  HIREDIS_INCLUDE_DIRS, the folders containing hiredis development files
#  HIREDIS_FOUND, whether hiredis was found on system

set(HIREDIS_NAMES ${HIREDIS_NAMES} libhiredis.a hiredis)

# Try with manually given path
find_library(
  HIREDIS_LIBRARY
  NAMES ${HIREDIS_NAMES}
  HINTS ${HIREDIS_ROOT}
  PATH_SUFFIXES lib/)

find_path(
  HIREDIS_INCLUDE_DIR
  NAMES hiredis/hiredis.h
  HINTS ${HIREDIS_ROOT}
  PATH_SUFFIXES include/)

if(HIREDIS_LIBRARY)
  set(HIREDIS_FOUND "YES")
else()
  set(HIREDIS_FOUND "NO")
endif()

set(HIREDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR})
set(HIREDIS_LIBRARIES ${HIREDIS_LIBRARY})

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  hiredis DEFAULT_MSG HIREDIS_LIBRARY HIREDIS_INCLUDE_DIR)

mark_as_advanced(HIREDIS_LIBRARY HIREDIS_INCLUDE_DIR)
