if(IS_DIRECTORY "${SOURCE_ROOT}/src/LVUI")
    message(FATAL_ERROR "The source directory repeats the project name")
endif()
foreach(_file Compiler/Document.h Compiler/Parser.cpp Compiler/Compiler.h Compiler/Compiler.cpp
        Language/Language.h Language/Language.cpp Language/Keywords.def Style/StyleTokens.h
        Style/StyleTokens.cpp Style/StyleTokens.generated.inc Runtime/NativeBindings.h
        Runtime/NativeBindings.cpp Color/ColorSelectionSurface.h Color/ColorSelectionSurface.cpp
        Collections/NativeListView.h Editing/CodeEditorController.h Overlays/NativeModal.h)
    if(NOT EXISTS "${SOURCE_ROOT}/src/${_file}")
        message(FATAL_ERROR "Missing responsibility-grouped source: ${_file}")
    endif()
endforeach()
