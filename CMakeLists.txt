cmake_minimum_required(VERSION 3.11)
project(svgutils CXX C)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)

include(SVGUtilsMacros)

include_directories(include/ ${PROJECT_BINARY_DIR}/include/)
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

add_library(${PROJECT_NAME} ${LIB_SOURCES})

if (SVG_UTILS_WITH_CAIRO)
  target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs ${CAIRO_LIBRARIES} ${FONTCONFIG_LIBRARIES} Freetype::Freetype)
endif()

add_subdirectory(tools)

option(SVG_UTILS_WITH_EXAMPLES "Build svgutils examples" ON)
if (SVG_UTILS_WITH_EXAMPLES)
  add_subdirectory(examples)
endif()

option(SVG_UTILS_WITH_UNITTESTS "Build svgutils unit tests" ON)
if (SVG_UTILS_WITH_UNITTESTS)
  enable_testing()
  add_subdirectory(unittest)
endif()

option(SVG_UTILS_WITH_TESTS "Run svgutils regression tests in test target" ON)
if (SVG_UTILS_WITH_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Check and update source code formatting
file(GLOB_RECURSE CHECK_FORMAT_SOURCES
  include/svgutils/*.h
  lib/*.cc
  test/*.h test/*.cc
  unittest/*.h unittest/*.cc
  utils/*.h utils/*.cc
  )

# Source code formatting using clang-format
set(check_format_depends)
set(update_format_depends)
set(i 0)
foreach (file IN LISTS CHECK_FORMAT_SOURCES)
  add_custom_command(OUTPUT ${PROJECT_NAME}-check-format${i}
    COMMAND clang-format -sort-includes -style=llvm ${file} | diff -u ${file} -
    VERBATIM
    COMMENT "Checking format of ${file}..."
  )
  list(APPEND check_format_depends "${PROJECT_NAME}-check-format${i}")

  add_custom_command(OUTPUT ${PROJECT_NAME}-update-format${i}
    COMMAND clang-format -sort-includes -i -style=llvm ${file}
    VERBATIM
    COMMENT "Updating format of ${file}..."
  )
  list(APPEND update_format_depends "${PROJECT_NAME}-update-format${i}")

  math(EXPR i ${i}+1)
endforeach ()

add_custom_target(${PROJECT_NAME}-check-format DEPENDS ${check_format_depends})
set_target_properties(${PROJECT_NAME}-check-format PROPERTIES FOLDER "${PROJECT_NAME}")

add_custom_target(${PROJECT_NAME}-update-format DEPENDS ${update_format_depends})
set_target_properties(${PROJECT_NAME}-update-format PROPERTIES FOLDER "${PROJECT_NAME}")
