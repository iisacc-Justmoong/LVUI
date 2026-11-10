if(IS_DIRECTORY "${SOURCE_ROOT}/src/LVUI")
    message(FATAL_ERROR "The source directory repeats the project name")
endif()
foreach(_file Compiler/Document.h Compiler/Parser.cpp Compiler/Compiler.h Compiler/Compiler.cpp
        Language/Language.h Language/Language.cpp Language/Keywords.def Style/StyleTokens.h
        Style/StyleTokens.cpp Style/StyleTokens.generated.inc Runtime/NativeBindings.h
        Runtime/NativeBindings.cpp Color/ColorSelectionSurface.h
        Collections/NativeListView.h Editing/CodeEditorController.h Overlays/NativeModal.h Media/NativeIcon.h
        Python/CompilerBridge.h Python/CompilerBridge.cpp Python/ii_lvui/__init__.py)
    if(NOT EXISTS "${SOURCE_ROOT}/src/${_file}")
        message(FATAL_ERROR "Missing responsibility-grouped source: ${_file}")
    endif()
endforeach()
foreach(_file ColorSelectionSurface.cpp NativeImageLoader.cpp NativeImageSource.cpp NativeImageLoader.h NativeImageAsync.h)
    if(NOT EXISTS "${SOURCE_ROOT}/example/Integrations/App/${_file}")
        message(FATAL_ERROR "Missing application-owned composition example: ${_file}")
    endif()
endforeach()
foreach(_file Media/Image.h Media/NativeImage.h Controls/Avatar.h Controls/NativeAvatar.h Overlays/Sheet.h Overlays/NativeSheet.h)
    if(EXISTS "${SOURCE_ROOT}/src/${_file}")
        message(FATAL_ERROR "Migrated visual implementation remains in LVUI: ${_file}")
    endif()
endforeach()
