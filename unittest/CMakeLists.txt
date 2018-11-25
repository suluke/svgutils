set(GTEST_BUILD_DIR ${PROJECT_BINARY_DIR}/googletest)

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Add googletest directly to our build. This adds
# the following targets: gtest, gtest_main, gmock
# and gmock_main
add_subdirectory(../extern/gtest
                 "${GTEST_BUILD_DIR}")

include(GoogleTest)

function(add_svg_unittest TEST)
  add_executable(${TEST} ${ARGN})
  target_link_libraries(${TEST} PRIVATE gtest_main)
  gtest_discover_tests(${TEST})
endfunction()

add_svg_unittest(cli_args_test cli_args_test.cc)
target_link_libraries(cli_args_test PRIVATE stdc++fs)
add_svg_unittest(svg_logging_writer_test svg_logging_writer_test.cc)
