if(NOT DEFINED ACCLOUD_REPO_ROOT)
  message(FATAL_ERROR "test_embed_spirv.cmake requires ACCLOUD_REPO_ROOT")
endif()
if(NOT DEFINED ACCLOUD_BINARY_ROOT)
  message(FATAL_ERROR "test_embed_spirv.cmake requires ACCLOUD_BINARY_ROOT")
endif()

set(_cmake_lists "${ACCLOUD_REPO_ROOT}/accloud/CMakeLists.txt")
set(_embed_script "${ACCLOUD_REPO_ROOT}/accloud/cmake/EmbedSpirv.cmake")
if(NOT EXISTS "${_cmake_lists}" OR NOT EXISTS "${_embed_script}")
  message(FATAL_ERROR "SPIR-V embedding self-test cannot locate project CMake files")
endif()

file(READ "${_cmake_lists}" _project_cmake)
if(_project_cmake MATCHES [[-DINPUT="]])
  message(FATAL_ERROR "CMakeLists.txt must not embed quotes inside the -DINPUT value")
endif()
if(_project_cmake MATCHES [[-DOUTPUT="]])
  message(FATAL_ERROR "CMakeLists.txt must not embed quotes inside the -DOUTPUT value")
endif()

set(_work_dir "${ACCLOUD_BINARY_ROOT}/embed spirv selftest")
set(_input "${_work_dir}/input shader.spv")
set(_output "${_work_dir}/generated header.h")
file(REMOVE_RECURSE "${_work_dir}")
file(MAKE_DIRECTORY "${_work_dir}")

# EmbedSpirv.cmake only requires a 32-bit-word-aligned byte stream. A tiny
# four-byte fixture is sufficient to exercise path passing and header output.
file(WRITE "${_input}" "SPV!")
execute_process(
  COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${_input}"
          "-DOUTPUT=${_output}"
          -P "${_embed_script}"
  RESULT_VARIABLE _result
  OUTPUT_VARIABLE _stdout
  ERROR_VARIABLE _stderr
)
if(NOT _result EQUAL 0)
  message(FATAL_ERROR
    "EmbedSpirv.cmake failed for a path containing spaces (exit ${_result})\n${_stdout}${_stderr}")
endif()
if(NOT EXISTS "${_output}")
  message(FATAL_ERROR "EmbedSpirv.cmake did not create the requested output header")
endif()

file(READ "${_output}" _generated)
foreach(_token IN ITEMS
    "kSupportOverlapSpirv"
    "kSupportOverlapSpirvWordCount"
    "0x21565053u")
  if(NOT _generated MATCHES "${_token}")
    message(FATAL_ERROR "Generated SPIR-V header is missing expected token: ${_token}")
  endif()
endforeach()

message(STATUS "SPIR-V embedding self-test passed")
