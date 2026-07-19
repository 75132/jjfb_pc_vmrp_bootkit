# Emit build-info.json next to the launcher binary.
function(gwy_write_build_info target)
  string(TIMESTAMP _built_utc "%Y-%m-%dT%H:%M:%SZ" UTC)
  set(_build_type "${CMAKE_BUILD_TYPE}")
  if(NOT _build_type)
    set(_build_type "multi")
  endif()

  set(_info_path "${CMAKE_BINARY_DIR}/build-info.json")
  file(WRITE "${_info_path}" "{
  \"product\": \"gwy_launcher\",
  \"built_utc\": \"${_built_utc}\",
  \"cmake_version\": \"${CMAKE_VERSION}\",
  \"c_compiler_id\": \"${CMAKE_C_COMPILER_ID}\",
  \"c_compiler_version\": \"${CMAKE_C_COMPILER_VERSION}\",
  \"system\": \"${CMAKE_SYSTEM_NAME}\",
  \"processor\": \"${CMAKE_SYSTEM_PROCESSOR}\",
  \"build_type\": \"${_build_type}\",
  \"c_standard\": \"11\",
  \"target_arch_expected\": \"PE32/i386\",
  \"deps\": {
    \"sdl2\": \"2.0.10\",
    \"unicorn\": \"1.0.2-win32\"
  },
  \"legacy_lab_excluded\": true
}
")

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_info_path}"
            "$<TARGET_FILE_DIR:${target}>/build-info.json"
    COMMENT "Installing build-info.json beside ${target}"
  )
endfunction()
