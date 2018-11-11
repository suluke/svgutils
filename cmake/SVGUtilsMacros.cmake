function(add_svgutils_config CONFIG_FILE)
  get_filename_component(CONFIG_FILENAME ${CONFIG_FILE} NAME)
  string(REGEX REPLACE ".in$" "" CONFIG_REALNAME "${CONFIG_FILENAME}")
  configure_file(
    ${CONFIG_FILE}
    "${PROJECT_BINARY_DIR}/CMakeFiles/include/${PROJECT_NAME}/${CONFIG_FILENAME}"
    )
  file(GENERATE
    OUTPUT "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}/${CONFIG_REALNAME}"
    INPUT "${PROJECT_BINARY_DIR}/CMakeFiles/include/${PROJECT_NAME}/${CONFIG_FILENAME}"
    )
endfunction()
