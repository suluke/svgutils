set(SVGUTILS_UTILS_DIR ${CMAKE_CURRENT_LIST_DIR})

function(add_svg_util UTIL)
  add_executable(${UTIL} ${ARGN})
  target_include_directories(${UTIL} PRIVATE ${SVGUTILS_UTILS_DIR}/include/)
endfunction()

add_subdirectory(svg2cxx)
add_subdirectory(tedisex)
