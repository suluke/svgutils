cmake_minimum_required(VERSION 3.11)
set(PROJECTNAME svgutils)
project(${PROJECTNAME} CXX C)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
# VA_OPT macro needs c++2a or gnu extensions
if (NOT ${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_CXX_STANDARD 2a)
endif()

include_directories(include/)
add_compile_options(-Wall -Werror)

add_subdirectory(utils)

set(LIB_SOURCES lib/svg_utils.cc lib/svg_reader_writer.cc lib/css_utils.cc lib/plotlib.cc)

find_package(Cairo)
find_package(Freetype)
find_package(Fontconfig)
option(SVG_UTILS_WITH_CAIRO "Build with Cairo backend support" ON)
if (NOT CAIRO_FOUND OR NOT FREETYPE_FOUND OR NOT FONTCONFIG_FOUND)
  message(STATUS "Unable to find Cairo. Disabling Cairo support...")
  set(SVG_UTILS_WITH_CAIRO OFF CACHE BOOL "Build with Cairo backend support" FORCE)
endif()
if (SVG_UTILS_WITH_CAIRO)
  include_directories(${CAIRO_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
  list(APPEND LIB_SOURCES lib/svg_cairo.cc lib/freetype.cc)
endif()

add_library(${PROJECTNAME} ${LIB_SOURCES})

if (SVG_UTILS_WITH_CAIRO)
  target_link_libraries(${PROJECTNAME} PRIVATE stdc++fs ${CAIRO_LIBRARIES} ${FONTCONFIG_LIBRARIES} Freetype::Freetype)
endif()

function(add_svgutils_test NAME)
  add_executable(${NAME} ${ARGN})
  target_link_libraries(${NAME} PRIVATE ${PROJECTNAME})
endfunction()

add_svgutils_test(test test/main.cc)
add_svgutils_test(jstest test/jstest.cc)
add_svgutils_test(plottest test/plottest.cc)
if (SVG_UTILS_WITH_CAIRO)
  add_svgutils_test(cairotest test/cairotest.cc)
endif()

add_subdirectory(tools)

# Check and update source code formatting
file(GLOB_RECURSE CHECK_FORMAT_SOURCES
  include/svgutils/*.h
  lib/*.cc
  test/*.h test/*.cc
  utils/*.h utils/*.cc
  )

# Source code formatting using clang-format
set(check_format_depends)
set(update_format_depends)
set(i 0)
foreach (file IN LISTS CHECK_FORMAT_SOURCES)
  add_custom_command(OUTPUT ${PROJECTNAME}-check-format${i}
    COMMAND clang-format -sort-includes -style=llvm ${file} | diff -u ${file} -
    VERBATIM
    COMMENT "Checking format of ${file}..."
  )
  list(APPEND check_format_depends "${PROJECTNAME}-check-format${i}")

  add_custom_command(OUTPUT ${PROJECTNAME}-update-format${i}
    COMMAND clang-format -sort-includes -i -style=llvm ${file}
    VERBATIM
    COMMENT "Updating format of ${file}..."
  )
  list(APPEND update_format_depends "${PROJECTNAME}-update-format${i}")

  math(EXPR i ${i}+1)
endforeach ()

add_custom_target(${PROJECTNAME}-check-format DEPENDS ${check_format_depends})
set_target_properties(${PROJECTNAME}-check-format PROPERTIES FOLDER "${PROJECTNAME}")

add_custom_target(${PROJECTNAME}-update-format DEPENDS ${update_format_depends})
set_target_properties(${PROJECTNAME}-update-format PROPERTIES FOLDER "${PROJECTNAME}")
