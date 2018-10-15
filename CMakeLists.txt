cmake_minimum_required(VERSION 3.11)
project(svgutils CXX C)
set(CMAKE_CXX_STANDARD 17)
# VA_OPT macro needs c++2a or gnu extensions
if (NOT ${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_CXX_STANDARD 2a)
endif()

include_directories(include/)
add_compile_options(-Wall -Werror)

add_executable(test test/main.cc lib/svg_utils.cc)
add_executable(jstest test/jstest.cc lib/svg_utils.cc)
