#[=======================================================================[.rst:
FindHiredis
-------

Finds the Hiredis library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Hiredis::Hiredis``
  The Hiredis library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Hiredis_FOUND``
  True if the system has the Hiredis library.
``Hiredis_VERSION``
  The version of the Hiredis library which was found.
``Hiredis_INCLUDE_DIRS``
  Include directories needed to use Hiredis.
``Hiredis_LIBRARIES``
  Libraries needed to link to Hiredis.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Hiredis_INCLUDE_DIR``
  The directory containing ``foo.h``.
``Hiredis_LIBRARY``
  The path to the Hiredis library.

#]=======================================================================]

find_package(PkgConfig)
pkg_check_modules(PC_Hiredis QUIET hiredis)


find_path(Hiredis_INCLUDE_DIR
  NAMES hiredis/hiredis.h
  PATHS ${PC_Hiredis_INCLUDE_DIRS}
  HINTS ${HIREDIS_ROOT}
  PATH_SUFFIXES usr/local/include/
)
find_library(Hiredis_LIBRARY
  NAMES libhiredis.a hiredis
  PATHS ${PC_Hiredis_LIBRARY_DIRS}
  HINTS ${HIREDIS_ROOT}
  PATH_SUFFIXES usr/local/lib/
)

set(Hiredis_VERSION ${PC_Hiredis_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	Hiredis
  FOUND_VAR Hiredis_FOUND
  REQUIRED_VARS
    Hiredis_LIBRARY
    Hiredis_INCLUDE_DIR
  VERSION_VAR Hiredis_VERSION
)

if(Hiredis_FOUND)
  set(Hiredis_LIBRARIES ${Hiredis_LIBRARY})
  set(Hiredis_INCLUDE_DIRS ${Hiredis_INCLUDE_DIR})
  set(Hiredis_DEFINITIONS ${PC_Hiredis_CFLAGS_OTHER})
endif()

if(Hiredis_FOUND AND NOT TARGET Hiredis::Hiredis)
  add_library(Hiredis::Hiredis UNKNOWN IMPORTED)
  set_target_properties(Hiredis::Hiredis PROPERTIES
    IMPORTED_LOCATION "${Hiredis_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_Hiredis_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Hiredis_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  Hiredis_INCLUDE_DIR
  Hiredis_LIBRARY
)

# compatibility variables
set(Hiredis_VERSION_STRING ${Hiredis_VERSION})