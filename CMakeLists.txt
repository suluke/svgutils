cmake_minimum_required(VERSION 3.11)
project(svgutils CXX C)
set(CMAKE_CXX_STANDARD 17)
# VA_OPT macro needs c++2a or gnu extensions
if (NOT ${CMAKE_COMPILER_IS_GNUCXX})
  set(CMAKE_CXX_STANDARD 2a)
endif()

add_executable(test test/main.cc lib/svg_utils.cc)

target_include_directories(test PRIVATE include/)
target_compile_options(test PRIVATE -Wall -Werror)
