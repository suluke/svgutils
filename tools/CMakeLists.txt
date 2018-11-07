set(SVGUTILS_TOOLS_DIR ${CMAKE_CURRENT_LIST_DIR})

function(add_svg_tool TOOL)
  add_executable(${TOOL} ${ARGN})
  target_include_directories(${TOOL} PRIVATE ${SVGUTILS_TOOLS_DIR}/include/)
  target_link_libraries(${TOOL} PRIVATE ${PROJECTNAME})
endfunction()

add_subdirectory(svgfmt)
if (SVG_UTILS_WITH_CAIRO)
  add_subdirectory(svg2pdf)
  add_subdirectory(svg2png)
endif()
