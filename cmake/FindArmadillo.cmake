# - Find Armadillo
# Find the Armadillo C++ library
#
# Using Armadillo:
#  find_package(Armadillo REQUIRED)
#  include_directories(${ARMADILLO_INCLUDE_DIRS})
#  add_executable(foo foo.cc)
#  target_link_libraries(foo ${ARMADILLO_LIBRARIES})
# This module sets the following variables:
#  ARMADILLO_FOUND - set to true if the library is found
#  ARMADILLO_INCLUDE_DIRS - list of required include directories
#  ARMADILLO_LIBRARIES - list of libraries to be linked
#  ARMADILLO_VERSION_MAJOR - major version number
#  ARMADILLO_VERSION_MINOR - minor version number
#  ARMADILLO_VERSION_PATCH - patch version number
#  ARMADILLO_VERSION_STRING - version number as a string (ex: "1.0.4")
#  ARMADILLO_VERSION_NAME - name of the version (ex: "Antipodean Antileech")

#=============================================================================
# Copyright 2011 Clement Creusot <creusot@cs.york.ac.uk>
#
#
# CMake - Cross Platform Makefile Generator
# Copyright 2000-2020 Kitware, Inc. and Contributors
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Kitware, Inc. nor the names of Contributors
#   may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ------------------------------------------------------------------------------
#
# The following individuals and institutions are among the Contributors:
#
# * Aaron C. Meadows <cmake@shadowguarddev.com>
# * Adriaan de Groot <groot@kde.org>
# * Aleksey Avdeev <solo@altlinux.ru>
# * Alexander Neundorf <neundorf@kde.org>
# * Alexander Smorkalov <alexander.smorkalov@itseez.com>
# * Alexey Sokolov <sokolov@google.com>
# * Alex Merry <alex.merry@kde.org>
# * Alex Turbov <i.zaufi@gmail.com>
# * Andreas Pakulat <apaku@gmx.de>
# * Andreas Schneider <asn@cryptomilk.org>
# * André Rigland Brodtkorb <Andre.Brodtkorb@ifi.uio.no>
# * Axel Huebl, Helmholtz-Zentrum Dresden - Rossendorf
# * Benjamin Eikel
# * Bjoern Ricks <bjoern.ricks@gmail.com>
# * Brad Hards <bradh@kde.org>
# * Christopher Harvey
# * Christoph Grüninger <foss@grueninger.de>
# * Clement Creusot <creusot@cs.york.ac.uk>
# * Daniel Blezek <blezek@gmail.com>
# * Daniel Pfeifer <daniel@pfeifer-mail.de>
# * Enrico Scholz <enrico.scholz@informatik.tu-chemnitz.de>
# * Eran Ifrah <eran.ifrah@gmail.com>
# * Esben Mose Hansen, Ange Optimization ApS
# * Geoffrey Viola <geoffrey.viola@asirobots.com>
# * Google Inc
# * Gregor Jasny
# * Helio Chissini de Castro <helio@kde.org>
# * Ilya Lavrenov <ilya.lavrenov@itseez.com>
# * Insight Software Consortium <insightsoftwareconsortium.org>
# * Jan Woetzel
# * Julien Schueller
# * Kelly Thompson <kgt@lanl.gov>
# * Laurent Montel <montel@kde.org>
# * Konstantin Podsvirov <konstantin@podsvirov.pro>
# * Mario Bensi <mbensi@ipsquad.net>
# * Martin Gräßlin <mgraesslin@kde.org>
# * Mathieu Malaterre <mathieu.malaterre@gmail.com>
# * Matthaeus G. Chajdas
# * Matthias Kretz <kretz@kde.org>
# * Matthias Maennich <matthias@maennich.net>
# * Michael Hirsch, Ph.D. <www.scivision.co>
# * Michael Stürmer
# * Miguel A. Figueroa-Villanueva
# * Mike Jackson
# * Mike McQuaid <mike@mikemcquaid.com>
# * Nicolas Bock <nicolasbock@gmail.com>
# * Nicolas Despres <nicolas.despres@gmail.com>
# * Nikita Krupen'ko <krnekit@gmail.com>
# * NVIDIA Corporation <www.nvidia.com>
# * OpenGamma Ltd. <opengamma.com>
# * Patrick Stotko <stotko@cs.uni-bonn.de>
# * Per Øyvind Karlsen <peroyvind@mandriva.org>
# * Peter Collingbourne <peter@pcc.me.uk>
# * Petr Gotthard <gotthard@honeywell.com>
# * Philip Lowman <philip@yhbt.com>
# * Philippe Proulx <pproulx@efficios.com>
# * Raffi Enficiaud, Max Planck Society
# * Raumfeld <raumfeld.com>
# * Roger Leigh <rleigh@codelibre.net>
# * Rolf Eike Beer <eike@sf-mail.de>
# * Roman Donchenko <roman.donchenko@itseez.com>
# * Roman Kharitonov <roman.kharitonov@itseez.com>
# * Ruslan Baratov
# * Sebastian Holtermann <sebholt@xwmw.org>
# * Stephen Kelly <steveire@gmail.com>
# * Sylvain Joubert <joubert.sy@gmail.com>
# * The Qt Company Ltd.
# * Thomas Sondergaard <ts@medical-insight.com>
# * Tobias Hunger <tobias.hunger@qt.io>
# * Todd Gamblin <tgamblin@llnl.gov>
# * Tristan Carel
# * University of Dundee
# * Vadim Zhukov
# * Will Dicharry <wdicharry@stellarscience.com>
#
# See version control history for details of individual contributions.
#
# The above copyright and license notice applies to distributions of
# CMake in source and binary form.  Third-party software packages supplied
# with CMake under compatible licenses provide their own copyright notices
# documented in corresponding subdirectories or source files.
#
# ------------------------------------------------------------------------------
#
# CMake was initially developed by Kitware with the following sponsorship:
#
#  * National Library of Medicine at the National Institutes of Health
#    as part of the Insight Segmentation and Registration Toolkit (ITK).
#
#  * US National Labs (Los Alamos, Livermore, Sandia) ASC Parallel
#    Visualization Initiative.
#
#  * National Alliance for Medical Image Computing (NAMIC) is funded by the
#    National Institutes of Health through the NIH Roadmap for Medical Research,
#    Grant U54 EB005149.
#
#  * Kitware, Inc.


find_path(ARMADILLO_INCLUDE_DIR
  NAMES armadillo
  PATHS "$ENV{ProgramFiles}/Armadillo/include"
  )


if(ARMADILLO_INCLUDE_DIR)
  # ------------------------------------------------------------------------
  #  Extract version information from <armadillo>
  # ------------------------------------------------------------------------

  # WARNING: Early releases of Armadillo didn't have the arma_version.hpp file.
  # (e.g. v.0.9.8-1 in ubuntu maverick packages (2001-03-15))
  # If the file is missing, set all values to 0
  set(ARMADILLO_VERSION_MAJOR 0)
  set(ARMADILLO_VERSION_MINOR 0)
  set(ARMADILLO_VERSION_PATCH 0)
  set(ARMADILLO_VERSION_NAME "EARLY RELEASE")

  if(EXISTS "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/arma_version.hpp")

    # Read and parse armdillo version header file for version number
    file(READ "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/arma_version.hpp" _armadillo_HEADER_CONTENTS)
    string(REGEX REPLACE ".*#define ARMA_VERSION_MAJOR ([0-9]+).*" "\\1" ARMADILLO_VERSION_MAJOR "${_armadillo_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define ARMA_VERSION_MINOR ([0-9]+).*" "\\1" ARMADILLO_VERSION_MINOR "${_armadillo_HEADER_CONTENTS}")
    string(REGEX REPLACE ".*#define ARMA_VERSION_PATCH ([0-9]+).*" "\\1" ARMADILLO_VERSION_PATCH "${_armadillo_HEADER_CONTENTS}")

    # WARNING: The number of spaces before the version name is not one.
    string(REGEX REPLACE ".*#define ARMA_VERSION_NAME\ +\"([0-9a-zA-Z\ _-]+)\".*" "\\1" ARMADILLO_VERSION_NAME "${_armadillo_HEADER_CONTENTS}")

  endif()

  set(ARMADILLO_VERSION_STRING "${ARMADILLO_VERSION_MAJOR}.${ARMADILLO_VERSION_MINOR}.${ARMADILLO_VERSION_PATCH}")
endif ()


#======================

# Determine what support libraries are being used, and whether or not we need to
# link against them.  We need to look in config.hpp.
set(SUPPORT_INCLUDE_DIRS "")
set(SUPPORT_LIBRARIES "")
set(ARMA_NEED_LIBRARY true) # Assume true.
if(EXISTS "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/config.hpp")
  file(READ "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/config.hpp" _armadillo_CONFIG_CONTENTS)
  # ARMA_USE_WRAPPER
  string(REGEX MATCH "\r?\n[\t ]*#define[ \t]+ARMA_USE_WRAPPER[ \t]*\r?\n" ARMA_USE_WRAPPER "${_armadillo_CONFIG_CONTENTS}")

  # ARMA_USE_LAPACK
  string(REGEX MATCH "\r?\n[\t ]*#if[\t ]+!defined[(]ARMA_USE_LAPACK[)][\t ]*\r?\n[\t ]*#define[ \t]+ARMA_USE_LAPACK[ \t]*\r?\n" ARMA_USE_LAPACK "${_armadillo_CONFIG_CONTENTS}")

  # ARMA_USE_BLAS
  string(REGEX MATCH "\r?\n[\t ]*#if[\t ]+!defined[(]ARMA_USE_BLAS[)][\t ]*\r?\n[\t ]*#define[ \t]+ARMA_USE_BLAS[ \t]*\r?\n" ARMA_USE_BLAS "${_armadillo_CONFIG_CONTENTS}")
    # ARMA_USE_ARPACK
  # ARMA_USE_ARPACK
  string(REGEX MATCH "\r?\n[\t ]*#if[\t ]+!defined[(]ARMA_USE_ARPACK[)][\t ]*\r?\n[\t ]*#define[ \t]+ARMA_USE_ARPACK[ \t]*\r?\n" ARMA_USE_ARPACK "${_armadillo_CONFIG_CONTENTS}")

  # Look for #define ARMA_USE_HDF5.
  string(REGEX MATCH "\r?\n[\t ]*#if[\t ]+!defined[(]ARMA_USE_HDF5[)][\t ]*\r?\n[\t ]*#define[ \t]+ARMA_USE_HDF5[ \t]*\r?\n" ARMA_USE_HDF5 "${_armadillo_CONFIG_CONTENTS}")

  # If we aren't wrapping, things get a little more complex.
  if("${ARMA_USE_WRAPPER}" STREQUAL "")
    set(ARMA_NEED_LIBRARY false)
    message(STATUS "ARMA_USE_WRAPPER is not defined, so all dependencies of "
                   "Armadillo must be manually linked.")

    set(HAVE_LAPACK false)
    set(HAVE_BLAS   false)

    # Search for LAPACK/BLAS (or replacement).
    if ((NOT "${ARMA_USE_LAPACK}" STREQUAL "") AND
        (NOT "${ARMA_USE_BLAS}" STREQUAL ""))
      # In order of preference: MKL, ACML, OpenBLAS, ATLAS
      set(MKL_FIND_QUIETLY true)
      include(ARMA_FindMKL)
      set(ACMLMP_FIND_QUIETLY true)
      include(ARMA_FindACMLMP)
      set(ACML_FIND_QUIETLY true)
      include(ARMA_FindACML)

      if (MKL_FOUND)
        message(STATUS "Using MKL for LAPACK/BLAS: ${MKL_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${MKL_LIBRARIES}")
        set(HAVE_LAPACK true)
        set(HAVE_BLAS   true)
      elseif (ACMLMP_FOUND)
        message(STATUS "Using multi-core ACML libraries for LAPACK/BLAS:
            ${ACMLMP_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${ACMLMP_LIBRARIES}")
        set(HAVE_LAPACK true)
        set(HAVE_BLAS   true)
      elseif (ACML_FOUND)
        message(STATUS "Using ACML for LAPACK/BLAS: ${ACML_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${ACML_LIBRARIES}")
        set(HAVE_LAPACK true)
        set(HAVE_BLAS   true)
      endif ()
    endif ()

    # If we haven't found BLAS, try.
    if (NOT "${ARMA_USE_BLAS}" STREQUAL "" AND NOT HAVE_BLAS)
      # Search for BLAS.
      set(OpenBLAS_FIND_QUIETLY true)
      include(ARMA_FindOpenBLAS)
      set(CBLAS_FIND_QUIETLY true)
      include(ARMA_FindCBLAS)
      set(BLAS_FIND_QUIETLY true)
      include(ARMA_FindBLAS)

      if (OpenBLAS_FOUND)
        # Warn if ATLAS is found also.
        if (CBLAS_FOUND)
          message(STATUS "Warning: both OpenBLAS and ATLAS have been found; "
              "ATLAS will not be used.")
        endif ()
        message(STATUS "Using OpenBLAS for BLAS: ${OpenBLAS_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${OpenBLAS_LIBRARIES}")
        set(HAVE_BLAS true)
      elseif (CBLAS_FOUND)
        message(STATUS "Using ATLAS for BLAS: ${CBLAS_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${CBLAS_LIBRARIES}")
        set(SUPPORT_INCLUDE_DIRS "${SUPPORT_INCLUDE_DIRS}"
            "${CBLAS_INCLUDE_DIR}")
        set(HAVE_BLAS true)
      elseif (BLAS_FOUND)
        message(STATUS "Using standard BLAS: ${BLAS_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${BLAS_LIBRARIES}")
        set(HAVE_BLAS true)
      endif ()
    endif ()

    # If we haven't found LAPACK, try.
    if (NOT "${ARMA_USE_LAPACK}" STREQUAL "" AND NOT HAVE_LAPACK)
      # Search for LAPACK.
      set(CLAPACK_FIND_QUIETLY true)
      include(ARMA_FindCLAPACK)
      set(LAPACK_FIND_QUIETLY true)
      include(ARMA_FindLAPACK)

      # Only use ATLAS if OpenBLAS isn't being used.
      if (CLAPACK_FOUND AND NOT OpenBLAS_FOUND)
        message(STATUS "Using ATLAS for LAPACK: ${CLAPACK_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${CLAPACK_LIBRARIES}")
        set(SUPPORT_INCLUDE_DIRS "${SUPPORT_INCLUDE_DIRS}"
            "${CLAPACK_INCLUDE_DIR}")
        set(HAVE_LAPACK true)
      elseif (LAPACK_FOUND)
        message(STATUS "Using standard LAPACK: ${LAPACK_LIBRARIES}")

        set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${LAPACK_LIBRARIES}")
        set(HAVE_LAPACK true)
      endif ()
    endif ()

    if (NOT "${ARMA_USE_LAPACK}" STREQUAL "" AND NOT HAVE_LAPACK)
      message(FATAL_ERROR "Cannot find LAPACK library, but ARMA_USE_LAPACK is "
          "set. Try specifying LAPACK libraries manually by setting the "
          "LAPACK_LIBRARY variable.")
    endif ()

    if (NOT "${ARMA_USE_BLAS}" STREQUAL "" AND NOT HAVE_BLAS)
      message(FATAL_ERROR "Cannot find BLAS library, but ARMA_USE_BLAS is set. "
          "Try specifying BLAS libraries manually by setting the BLAS_LIBRARY "
          "variable.")
    endif ()

    # Search for ARPACK (or replacement).
    if (NOT "${ARMA_USE_ARPACK}" STREQUAL "")
      # Use Armadillo ARPACK-finding procedure.
      set(ARPACK_FIND_QUIETLY true)
      include(ARMA_FindARPACK)

      if (NOT ARPACK_FOUND)
        message(FATAL_ERROR "ARMA_USE_ARPACK is defined in "
            "armadillo_bits/config.hpp, but ARPACK cannot be found.  Try "
            "specifying ARPACK_LIBRARY.")
      endif ()

      set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${ARPACK_LIBRARY}")
    endif ()

    # Search for HDF5 (or replacement).
    if (NOT "${ARMA_USE_HDF5}" STREQUAL "")
      find_package(HDF5 QUIET)

      if(NOT HDF5_FOUND)
        # On Debian systems, the HDF5 package has been split into multiple
        # packages so that it is co-installable.  But this may mean that the
        # include files are hidden somewhere very odd that the FindHDF5.cmake
        # script will not find.  Thus, we'll also quickly check pkgconfig to see
        # if there is information on what to use there.
        find_package(PkgConfig)
        if (PKG_CONFIG_FOUND)
          pkg_check_modules(HDF5 hdf5)
          # But using pkgconfig is a little weird because HDF5_LIBRARIES won't
          # be filled with exact library paths, like the other scripts.  So
          # instead what we get is HDF5_LIBRARY_DIRS which is the equivalent of
          # what we'd pass to -L.
          if (HDF5_FOUND)
            # I'm not sure what I think of doing this here...
            link_directories("${HDF5_LIBRARY_DIRS}")
          endif()
        endif()
      endif()

      if(NOT HDF5_FOUND)
        # We tried but didn't find it.
        message(FATAL_ERROR "Armadillo HDF5 support is enabled, but HDF5 "
            "cannot be found on the system.  Consider disabling HDF5 support.")
      endif()

      set(SUPPORT_INCLUDE_DIRS "${SUPPORT_INCLUDE_DIRS}" "${HDF5_INCLUDE_DIRS}")
      set(SUPPORT_LIBRARIES "${SUPPORT_LIBRARIES}" "${HDF5_LIBRARIES}")
    endif ()

  else()
    # Some older versions still require linking against HDF5 since they did not
    # wrap libhdf5.  This was true for versions older than 4.300.
    if(NOT "${ARMA_USE_HDF5}" STREQUAL "" AND
       "${ARMADILLO_VERSION_STRING}" VERSION_LESS "4.300.0")
      message(STATUS "Armadillo HDF5 support is enabled and manual linking is "
                     "required.")
      # We have HDF5 support and need to link against HDF5.
      find_package(HDF5)

      if(NOT HDF5_FOUND)
        # On Debian systems, the HDF5 package has been split into multiple
        # packages so that it is co-installable.  But this may mean that the
        # include files are hidden somewhere very odd that the FindHDF5.cmake
        # script will not find.  Thus, we'll also quickly check pkgconfig to see
        # if there is information on what to use there.
        find_package(PkgConfig)
        if (PKG_CONFIG_FOUND)
          pkg_check_modules(HDF5 hdf5)
          # But using pkgconfig is a little weird because HDF5_LIBRARIES won't
          # be filled with exact library paths, like the other scripts.  So
          # instead what we get is HDF5_LIBRARY_DIRS which is the equivalent of
          # what we'd pass to -L.
          if (HDF5_FOUND)
            # I'm not sure what I think of doing this here...
            link_directories("${HDF5_LIBRARY_DIRS}")
          endif()
        endif()
      endif()

      if(NOT HDF5_FOUND)
        # We tried but didn't find it.
        message(FATAL_ERROR "Armadillo HDF5 support is enabled, but HDF5 "
            "cannot be found on the system.  Consider disabling HDF5 support.")
      endif()

      set(SUPPORT_INCLUDE_DIRS "${HDF5_INCLUDE_DIRS}")
      set(SUPPORT_LIBRARIES "${HDF5_LIBRARIES}")
    endif()

    # Versions between 4.300 and 4.500 did successfully wrap HDF5, but didn't have good support for setting the include directory correctly.
    if(NOT "${ARMA_USE_HDF5}" STREQUAL "" AND
       "${ARMADILLO_VERSION_STRING}" VERSION_GREATER "4.299.0" AND
       "${ARMADILLO_VERSION_STRING}" VERSION_LESS "4.450.0")
      message(STATUS "Armadillo HDF5 support is enabled and include "
                     "directories must be found.")
      find_package(HDF5)

      if(NOT HDF5_FOUND)
        # On Debian systems, the HDF5 package has been split into multiple
        # packages so that it is co-installable.  But this may mean that the
        # include files are hidden somewhere very odd that the FindHDF5.cmake
        # script will not find.  Thus, we'll also quickly check pkgconfig to see
        # if there is information on what to use there.
        find_package(PkgConfig)
        if (PKG_CONFIG_FOUND)
          pkg_check_modules(HDF5 hdf5)
        endif()
      endif()

      if(NOT HDF5_FOUND)
        # We tried but didn't find it.
        message(FATAL_ERROR "Armadillo HDF5 support is enabled, but HDF5 "
            "cannot be found on the system.  Consider disabling HDF5 support.")
      endif()

      set(SUPPORT_INCLUDE_DIRS "${HDF5_INCLUDE_DIRS}")
    endif()

  endif()
else()
  message(FATAL_ERROR "${ARMADILLO_INCLUDE_DIR}/armadillo_bits/config.hpp not "
      "found!  Cannot determine what to link against.")
endif()

if (ARMA_NEED_LIBRARY)
  # UNIX paths are standard, no need to write.
  find_library(ARMADILLO_LIBRARY
    NAMES armadillo
    PATHS "$ENV{ProgramFiles}/Armadillo/lib"  "$ENV{ProgramFiles}/Armadillo/lib64" "$ENV{ProgramFiles}/Armadillo"
    )

  # Checks 'REQUIRED', 'QUIET' and versions.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Armadillo
    REQUIRED_VARS ARMADILLO_LIBRARY ARMADILLO_INCLUDE_DIR
    VERSION_VAR ARMADILLO_VERSION_STRING)
  # version_var fails with cmake < 2.8.4.
else ()
  # Checks 'REQUIRED', 'QUIET' and versions.
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(Armadillo
    REQUIRED_VARS ARMADILLO_INCLUDE_DIR
    VERSION_VAR ARMADILLO_VERSION_STRING)
endif ()

if (ARMADILLO_FOUND)
  # Also include support include directories.
  set(ARMADILLO_INCLUDE_DIRS ${ARMADILLO_INCLUDE_DIR} ${SUPPORT_INCLUDE_DIRS})
  # Also include support libraries to link against.
  if (ARMA_NEED_LIBRARY)
    set(ARMADILLO_LIBRARIES ${ARMADILLO_LIBRARY} ${SUPPORT_LIBRARIES})
  else ()
    set(ARMADILLO_LIBRARIES ${SUPPORT_LIBRARIES})
  endif ()
  message(STATUS "Armadillo libraries: ${ARMADILLO_LIBRARIES}")
endif ()


# Hide internal variables
mark_as_advanced(
  ARMADILLO_INCLUDE_DIR
  ARMADILLO_LIBRARY)

#======================