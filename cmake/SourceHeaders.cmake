# Map the organized authored files to the stable public LVUI include namespace.
foreach(_area IN ITEMS Compiler Language Style Editing Color Collections Overlays Navigation Media Layout Runtime Controls)
    file(GLOB _headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/${_area}/*.h")
    foreach(_header IN LISTS _headers)
        get_filename_component(_name "${_header}" NAME)
        if(DEFINED _lvui_header_${_name})
            message(FATAL_ERROR "Duplicate LVUI header basename: ${_name}")
        endif()
        set("_lvui_header_${_name}" "${_header}")
        file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/LVUI/${_name}"
            CONTENT "#pragma once\n#include \"${_header}\"\n")
    endforeach()
endforeach()
