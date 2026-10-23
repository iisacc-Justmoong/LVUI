set(_arguments --name "${LVUI_AOT_NAME}")
if(LVUI_AOT_FRAMEWORK)
    list(APPEND _arguments --framework "${LVUI_AOT_FRAMEWORK}")
endif()
foreach(_header IN LISTS LVUI_AOT_INCLUDES)
    list(APPEND _arguments --include "${_header}")
endforeach()
execute_process(COMMAND "${LVUI_AOT_COMPILER}" ${_arguments} "${LVUI_AOT_SOURCE}"
    RESULT_VARIABLE _status OUTPUT_VARIABLE _code ERROR_VARIABLE _error)
if(NOT _status EQUAL 0)
    message(FATAL_ERROR "LVUI native compilation failed (${_status}):\n${_error}")
endif()
# CMake owns this generated build artifact. Never replace it on compiler failure.
file(WRITE "${LVUI_AOT_OUTPUT}" "${_code}")
