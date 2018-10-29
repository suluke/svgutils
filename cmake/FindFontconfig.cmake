# - Try to find Fontconfig
# Once done, this will define
#
#  FONTCONFIG_FOUND - system has Fontconfig
#  FONTCONFIG_INCLUDE_DIRS - the Fontconfig include directories
#  FONTCONFIG_LIBRARIES - link these to use Fontconfig
#
# This file is based on the example listed on
# https://gitlab.kitware.com/cmake/community/wikis/doc/tutorials/How-To-Find-Libraries#using-libfindmacros

include(LibFindMacros)

# Dependencies
#libfind_package(FONTCONFIG)

# Use pkg-config to get hints about paths
#libfind_pkg_check_modules(FONTCONFIG_PKGCONF)

# Include dir
find_path(FONTCONFIG_INCLUDE_DIR
  NAMES fontconfig/fontconfig.h
  PATHS ${FONTCONFIG_PKGCONF_INCLUDE_DIRS}
)

# Finally the library itself
find_library(FONTCONFIG_LIBRARY
  NAMES fontconfig
  PATHS ${FONTCONFIG_PKGCONF_LIBRARY_DIRS}
)

# Set the include dir variables and the libraries and let libfind_process do the rest.
# NOTE: Singular variables for this library, plural for libraries this this lib depends on.
set(FONTCONFIG_PROCESS_INCLUDES FONTCONFIG_INCLUDE_DIR)
set(FONTCONFIG_PROCESS_LIBS FONTCONFIG_LIBRARY)
libfind_process(FONTCONFIG)
