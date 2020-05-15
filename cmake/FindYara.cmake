#[=======================================================================[.rst:
FindYara
-------

Finds the Yara library.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``Yara::Yara``
  The Yara library

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Yara_FOUND``
  True if the system has the Yara library.
``Yara_VERSION``
  The version of the Yara library which was found.
``Yara_INCLUDE_DIRS``
  Include directories needed to use Yara.
``Yara_LIBRARIES``
  Libraries needed to link to Yara.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Yara_INCLUDE_DIR``
  The directory containing ``foo.h``.
``Yara_LIBRARY``
  The path to the Yara library.

#]=======================================================================]

set(YARA_NAMES ${YARA_NAMES} yara libyara.a)

find_package(PkgConfig)
pkg_check_modules(PC_Yara QUIET yara crypto)

find_path(Yara_INCLUDE_DIR
  NAMES yara.h
  PATHS ${PC_Yara_INCLUDE_DIRS}
  HINTS ${YARA_ROOT}
  PATH_SUFFIXES include/
)
  find_library(Yara_LIBRARY
  NAMES ${YARA_NAMES}
  PATHS ${PC_Yara_LIBRARY_DIRS}
  HINTS ${YARA_ROOT}
  PATH_SUFFIXES lib/
)

set(Yara_VERSION ${PC_Yara_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
	Yara
  FOUND_VAR Yara_FOUND
  REQUIRED_VARS
    Yara_LIBRARY
    Yara_INCLUDE_DIR
  VERSION_VAR Yara_VERSION
)

if(Yara_FOUND)
  set(Yara_LIBRARIES ${Yara_LIBRARY})
  set(Yara_INCLUDE_DIRS ${Yara_INCLUDE_DIR})
  set(Yara_DEFINITIONS ${PC_Yara_CFLAGS_OTHER})
endif()

if(Yara_FOUND AND NOT TARGET Yara::Yara)
  add_library(Yara::Yara UNKNOWN IMPORTED)
  set_target_properties(Yara::Yara PROPERTIES
    IMPORTED_LOCATION "${Yara_LIBRARY}"
    INTERFACE_COMPILE_OPTIONS "${PC_Yara_CFLAGS_OTHER}"
    INTERFACE_INCLUDE_DIRECTORIES "${Yara_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(
  Yara_INCLUDE_DIR
  Yara_LIBRARY
)

# compatibility variables
set(Yara_VERSION_STRING ${Yara_VERSION})