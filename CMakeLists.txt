cmake_minimum_required(VERSION 3.11)
set(PROJECTNAME svgutils)
project(${PROJECTNAME} CXX C)
set(CMAKE_CXX_STANDARD 17)
# VA_OPT macro needs c++2a or gnu extensions
if (NOT ${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_CXX_STANDARD 2a)
endif()

include_directories(include/)
add_compile_options(-Wall -Werror)

add_executable(test test/main.cc lib/svg_utils.cc)
add_executable(jstest test/jstest.cc lib/svg_utils.cc)
add_executable(plottest test/plottest.cc lib/plotlib.cc lib/svg_utils.cc)

# Check and update source code formatting
file(GLOB_RECURSE CHECK_FORMAT_SOURCES
  include/svgutils/*.h
  lib/*.cc
  test/*.h test/*.cc
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
