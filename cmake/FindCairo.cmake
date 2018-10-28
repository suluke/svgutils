# - Try to find Cairo
# Once done, this will define
#
#  CAIRO_FOUND - system has Cairo
#  CAIRO_INCLUDE_DIRS - the Cairo include directories
#  CAIRO_LIBRARIES - link these to use Cairo
#
# This file is based on the example listed on
# https://gitlab.kitware.com/cmake/community/wikis/doc/tutorials/How-To-Find-Libraries#using-libfindmacros

include(LibFindMacros)

# Dependencies
#libfind_package(CAIRO)

# Use pkg-config to get hints about paths
#libfind_pkg_check_modules(CAIRO_PKGCONF)

# Include dir
find_path(CAIRO_INCLUDE_DIR
  NAMES cairo/cairo.h
  PATHS ${CAIRO_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(CAIRO_LIBRARY
  NAMES cairo
  PATHS ${CAIRO_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(CAIRO_PROCESS_INCLUDES CAIRO_INCLUDE_DIR)
set(CAIRO_PROCESS_LIBS CAIRO_LIBRARY)
libfind_process(CAIRO)
