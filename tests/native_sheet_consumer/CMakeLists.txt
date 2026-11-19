cmake_minimum_required(VERSION 3.31)
project(LVUINativeSheetConsumer LANGUAGES CXX)

include(CTest)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI ${LVUI_IIGUI_MIN_VERSION} CONFIG REQUIRED)
set(IIGUI_WIDGET_STATE_HEADER "" CACHE FILEPATH "iiGUI private WidgetState.h for the headless test host")
if(NOT EXISTS "${IIGUI_WIDGET_STATE_HEADER}")
    message(FATAL_ERROR "IIGUI_WIDGET_STATE_HEADER must identify the headless test host")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/private/iiGUI/WidgetState.h"
    CONTENT "#pragma once\n#include \"${IIGUI_WIDGET_STATE_HEADER}\"\n")
lvui_generate(NAME NativeOwnedSheet SOURCE ../fixtures/NativeOwnedSheet.lvui
    INCLUDES NativeOwnedSheetFixture.h OUTPUT_VARIABLE generated)
add_executable(LVUINativeSheetConsumer ../native_owned_sheet_generated.cpp "${generated}")
target_compile_features(LVUINativeSheetConsumer PRIVATE cxx_std_23)
target_include_directories(LVUINativeSheetConsumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures" "${CMAKE_CURRENT_BINARY_DIR}/private")
target_link_libraries(LVUINativeSheetConsumer PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.NativeSheet.Consumer COMMAND LVUINativeSheetConsumer)
if(APPLE)
    set_tests_properties(LVUI.NativeSheet.Consumer PROPERTIES ENVIRONMENT_MODIFICATION
        "DYLD_LIBRARY_PATH=unset:;DYLD_FALLBACK_LIBRARY_PATH=unset:;DYLD_FRAMEWORK_PATH=unset:;DYLD_FALLBACK_FRAMEWORK_PATH=unset:")
endif()
