include_guard(GLOBAL)
function(lvui_generate)
    set(_runner "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RunLVUICompiler.cmake")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;SOURCE;OUTPUT_VARIABLE;FRAMEWORK;STYLE_FILE" "INCLUDES;STYLE_MODES")
    if(ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES OR NOT ARG_NAME MATCHES "^[A-Za-z][A-Za-z0-9_]*$" OR NOT ARG_SOURCE OR NOT ARG_OUTPUT_VARIABLE)
        message(FATAL_ERROR "lvui_generate requires NAME, SOURCE, OUTPUT_VARIABLE and optional INCLUDES")
    endif()
    if(ARG_FRAMEWORK AND NOT ARG_FRAMEWORK STREQUAL "iiGUI")
        message(FATAL_ERROR "Unsupported native framework '${ARG_FRAMEWORK}'; LVUI uses iiGUI")
    endif()
    if(LVUI_HOST_COMPILER)
        set(_compiler "${LVUI_HOST_COMPILER}")
        set(_dependency "${LVUI_HOST_COMPILER}")
    elseif(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR "Cross compilation requires LVUI_HOST_COMPILER pointing to a host-native lvui-compile")
    else()
        set(_compiler "$<TARGET_FILE:LVUI::lvui-compile>")
        set(_dependency LVUI::lvui-compile)
    endif()
    cmake_path(ABSOLUTE_PATH ARG_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE _source)
    set(_output "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.generated.h")
    set(_style_file "")
    if(ARG_STYLE_FILE)
        cmake_path(ABSOLUTE_PATH ARG_STYLE_FILE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE _style_file)
    endif()
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${_compiler}" "-DLVUI_AOT_SOURCE=${_source}"
            "-DLVUI_AOT_OUTPUT=${_output}" "-DLVUI_AOT_NAME=${ARG_NAME}" "-DLVUI_AOT_INCLUDES=${ARG_INCLUDES}"
            "-DLVUI_AOT_FRAMEWORK=${ARG_FRAMEWORK}"
            "-DLVUI_AOT_STYLE_MODES=${ARG_STYLE_MODES}"
            "-DLVUI_AOT_STYLE_FILE=${_style_file}"
            -P "${_runner}"
        DEPENDS "${_source}" ${_style_file} ${_dependency} "${_runner}"
        VERBATIM COMMENT "AOT compiling ${ARG_NAME}.lvui into native C++23")
    set(${ARG_OUTPUT_VARIABLE} "${_output}" PARENT_SCOPE)
endfunction()
function(lvui_generate_styles)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;SOURCE;NAMESPACE;OUTPUT_VARIABLE" "STYLE_MODES")
    if(ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES OR NOT ARG_NAME MATCHES "^[A-Za-z][A-Za-z0-9_]*$" OR NOT ARG_SOURCE OR NOT ARG_OUTPUT_VARIABLE)
        message(FATAL_ERROR "lvui_generate_styles requires NAME, SOURCE, OUTPUT_VARIABLE and optional NAMESPACE")
    endif()
    if(CMAKE_CROSSCOMPILING AND NOT LVUI_HOST_COMPILER)
        message(FATAL_ERROR "Cross compilation requires LVUI_HOST_COMPILER")
    endif()
    if(LVUI_HOST_COMPILER)
        set(_compiler "${LVUI_HOST_COMPILER}")
        set(_dependency "${LVUI_HOST_COMPILER}")
    else()
        set(_compiler "$<TARGET_FILE:LVUI::lvui-compile>")
        set(_dependency LVUI::lvui-compile)
    endif()
    cmake_path(ABSOLUTE_PATH ARG_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE _source)
    set(_output "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.generated.h")
    set(_runner "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RunLVUICompiler.cmake")
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" "-DLVUI_AOT_COMPILER=${_compiler}" "-DLVUI_AOT_SOURCE=${_source}"
            "-DLVUI_AOT_OUTPUT=${_output}" "-DLVUI_AOT_NAME=${ARG_NAME}"
            "-DLVUI_AOT_STYLE_MODES=${ARG_STYLE_MODES}"
            "-DLVUI_AOT_EMIT_STYLES=ON" "-DLVUI_AOT_NAMESPACE=${ARG_NAMESPACE}" -P "${_runner}"
        DEPENDS "${_source}" ${_dependency} "${_runner}" VERBATIM)
    set(${ARG_OUTPUT_VARIABLE} "${_output}" PARENT_SCOPE)
endfunction()
