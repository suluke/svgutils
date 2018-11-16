set(TEDISEX_IGNORE_PATTERNS "Inputs/" "CMakeLists.txt")
set(TEDISEX_CONSIDER_PATTERNS ".svg" ".test" ".txt")
set(TEDISEX_FEATURES ${CMAKE_BUILD_TYPE})
add_svgutils_config(tedisex_config.h.in)

add_svg_util(tedisex tedisex.cc)
target_include_directories(tedisex SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/extern)
target_link_libraries(tedisex PRIVATE stdc++fs -pthread)
