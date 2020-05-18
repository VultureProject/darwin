# Find faup library
# This module defines
#  FAUP_LIBRARIES, the libraries needed to use faup
#  FAUP_INCLUDE_DIRS, the headers needed to use faup
#  FAUP_FOUND, whether faup was found on system

set(FAUP_NAMES ${FAUP_NAMES} libfaup_static.a faupl)

# Try with manually given path
find_library(
  FAUP_LIBRARY
  NAMES ${FAUP_NAMES}
  HINTS ${FAUP_ROOT}
  PATH_SUFFIXES lib/ usr/local/lib/)

find_path(
  FAUP_INCLUDE_DIR
  NAMES faup/faup.h
  HINTS ${FAUP_ROOT}
  PATH_SUFFIXES include/ usr/local/include/)

if(FAUP_LIBRARY)
  set(FAUP_FOUND "YES")
else()
  set(FAUP_FOUND "NO")
endif()

set(FAUP_LIBRARIES ${FAUP_LIBRARY})
set(FAUP_INCLUDE_DIRS ${FAUP_INCLUDE_DIR})

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Faup
  DEFAULT_MSG
  FAUP_LIBRARIES FAUP_INCLUDE_DIRS)

mark_as_advanced(FAUP_LIBRARY)
mark_as_advanced(FAUP_INCLUDE_DIR)
