cmake_minimum_required(VERSION 3.31)
project(VirtualVisibleConsumer LANGUAGES CXX)
include(CTest)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI ${LVUI_IIGUI_MIN_VERSION} CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(LVUI_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(IIGUI_SOURCE "${LVUI_SOURCE}/../iiGUI" ABSOLUTE)
lvui_generate(NAME NativeVirtualRepetitionRuntime
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeVirtualRepetition.lvui"
    FRAMEWORK iiGUI INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE generated)
lvui_generate(NAME NativeVisibleApplication
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeVisibleApplication.lvui"
    FRAMEWORK iiGUI INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE application_generated)
lvui_generate(NAME NativeVisibleConditional
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeVisibleConditional.lvui"
    FRAMEWORK iiGUI INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE conditional_generated)
lvui_generate(NAME NativeInputFocus
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeInputFocus.lvui"
    FRAMEWORK iiGUI OUTPUT_VARIABLE focus_generated)
lvui_generate(NAME NativeConditionalInputFocus
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeConditionalInputFocus.lvui"
    FRAMEWORK iiGUI INCLUDES NativeConditionalInputFocusFixture.h OUTPUT_VARIABLE conditional_focus_generated)
lvui_generate(NAME NativeOwnedInputField
    SOURCE "${LVUI_SOURCE}/tests/fixtures/NativeOwnedInputField.lvui"
    FRAMEWORK iiGUI INCLUDES NativeOwnedInputFieldFixture.h OUTPUT_VARIABLE owned_input_generated)
add_executable(VirtualVisibleConsumer "${IIGUI_SOURCE}/tests/native_virtual_repetition.cpp" "${generated}")
target_compile_features(VirtualVisibleConsumer PRIVATE cxx_std_23)
target_include_directories(VirtualVisibleConsumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${LVUI_SOURCE}/tests/fixtures")
target_link_libraries(VirtualVisibleConsumer PRIVATE LVUI::Native iiGUI::iiGUI)
add_executable(VirtualVisibleApplication application.cpp "${application_generated}")
target_compile_features(VirtualVisibleApplication PRIVATE cxx_std_23)
target_include_directories(VirtualVisibleApplication PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${LVUI_SOURCE}/tests/fixtures" "${IIGUI_SOURCE}/src/Widgets")
target_link_libraries(VirtualVisibleApplication PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.VirtualVisible.application COMMAND VirtualVisibleApplication)
add_executable(VirtualVisibleConditional conditional.cpp "${conditional_generated}")
target_compile_features(VirtualVisibleConditional PRIVATE cxx_std_23)
target_include_directories(VirtualVisibleConditional PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${LVUI_SOURCE}/tests/fixtures" "${IIGUI_SOURCE}/src/Widgets")
target_link_libraries(VirtualVisibleConditional PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.VirtualVisible.conditional COMMAND VirtualVisibleConditional)
add_executable(InputFocusConsumer input_focus.cpp "${focus_generated}")
target_compile_features(InputFocusConsumer PRIVATE cxx_std_23)
target_include_directories(InputFocusConsumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${IIGUI_SOURCE}/src/Widgets")
target_link_libraries(InputFocusConsumer PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.InputFocus.Consumer COMMAND InputFocusConsumer)
add_executable(ConditionalInputFocusConsumer conditional_input_focus.cpp "${conditional_focus_generated}")
target_compile_features(ConditionalInputFocusConsumer PRIVATE cxx_std_23)
target_include_directories(ConditionalInputFocusConsumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${LVUI_SOURCE}/tests/fixtures" "${IIGUI_SOURCE}/src/Widgets")
target_link_libraries(ConditionalInputFocusConsumer PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.InputFocus.Conditional COMMAND ConditionalInputFocusConsumer)
add_executable(OwnedInputFocusConsumer "${LVUI_SOURCE}/tests/native_owned_input_field_generated.cpp" "${owned_input_generated}")
target_compile_features(OwnedInputFocusConsumer PRIVATE cxx_std_23)
target_include_directories(OwnedInputFocusConsumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${LVUI_SOURCE}/tests/fixtures" "${IIGUI_SOURCE}/build/include")
target_link_libraries(OwnedInputFocusConsumer PRIVATE LVUI::Native iiGUI::iiGUI)
add_test(NAME LVUI.InputFocus.WithSubmit COMMAND OwnedInputFocusConsumer)
foreach(test viewport anchor resize rollback lifetime horizontal empty reentry autoheight visible_failure reveal_request)
    add_test(NAME LVUI.VirtualVisible.${test} COMMAND VirtualVisibleConsumer ${test})
    set_tests_properties(LVUI.VirtualVisible.${test} PROPERTIES TIMEOUT 30)
endforeach()
add_test(NAME LVUI.VirtualVisible.scope_allocation COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/check_codegen.py" "${CMAKE_CURRENT_BINARY_DIR}/NativeVirtualRepetitionRuntime.generated.h"
    "${CMAKE_CURRENT_BINARY_DIR}/NativeVisibleConditional.generated.h")
