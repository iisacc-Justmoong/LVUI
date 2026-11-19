include_guard(GLOBAL)

# Generate an external PaintView factory. COMPILER names a host-native path
# or a native-build executable target, such as LVUI::lvui-compile.
function(iigui_compile_lvui)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "NAME;SOURCE;COMPILER;STYLE_FILE;OUTPUT_VARIABLE" "INCLUDES;STYLE_MODES")
    if(ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES OR
       NOT ARG_NAME MATCHES "^[A-Za-z][A-Za-z0-9_]*$" OR NOT ARG_SOURCE OR
       NOT ARG_OUTPUT_VARIABLE MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
        message(FATAL_ERROR "iigui_compile_lvui requires NAME, SOURCE, OUTPUT_VARIABLE and a host compiler")
    endif()
    if(NOT ARG_COMPILER)
        set(ARG_COMPILER "${IIGUI_LVUI_COMPILER}")
    endif()
    if(NOT ARG_COMPILER)
        message(FATAL_ERROR "Provide COMPILER or IIGUI_LVUI_COMPILER as a host-native lvui-compile path")
    endif()
    cmake_path(ABSOLUTE_PATH ARG_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _source)
    if(NOT EXISTS "${_source}")
        message(FATAL_ERROR "The LVUI source must exist")
    endif()
    if(NOT CMAKE_CROSSCOMPILING AND TARGET "${ARG_COMPILER}")
        set(_compiler "$<TARGET_FILE:${ARG_COMPILER}>")
    else()
        cmake_path(ABSOLUTE_PATH ARG_COMPILER BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _compiler)
        if(NOT EXISTS "${_compiler}")
            message(FATAL_ERROR "The LVUI host compiler must exist")
        endif()
    endif()
    set(_style_file "")
    if(ARG_STYLE_FILE)
        cmake_path(ABSOLUTE_PATH ARG_STYLE_FILE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE _style_file)
        if(NOT EXISTS "${_style_file}")
            message(FATAL_ERROR "The LVUI style file must exist")
        endif()
    endif()
    set(_output "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.generated.h")
    set(_runner "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/iiGUIRunLvui.cmake")
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${CMAKE_COMMAND}" "-DIIGUI_LVUI_COMPILER=${_compiler}" "-DIIGUI_LVUI_SOURCE=${_source}"
            "-DIIGUI_LVUI_OUTPUT=${_output}" "-DIIGUI_LVUI_NAME=${ARG_NAME}"
            "-DIIGUI_LVUI_INCLUDES=${ARG_INCLUDES}" "-DIIGUI_LVUI_STYLE_MODES=${ARG_STYLE_MODES}"
            "-DIIGUI_LVUI_STYLE_FILE=${_style_file}" -P "${_runner}"
        DEPENDS "${_source}" "${_compiler}" ${_style_file} "${_runner}"
        COMMENT "AOT compiling ${ARG_NAME}.lvui for iiGUI" VERBATIM)
    set(${ARG_OUTPUT_VARIABLE} "${_output}" PARENT_SCOPE)
endfunction()
