cmake_minimum_required(VERSION 3.31)
project(LVUIWebConsumer LANGUAGES CXX)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI ${LVUI_IIGUI_MIN_VERSION} CONFIG REQUIRED)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../example/Web" web-example)

if(TARGET LVUI::CompilerBridge OR LVUI_CompilerBridge_FOUND)
    message(FATAL_ERROR "A target-only Web package unexpectedly contains the host Python compiler bridge")
endif()
# Compile an existing listener fixture as well: it includes viewport callbacks,
# observable dimensions and several component bindings through the real AOT.
lvui_generate(NAME NativeOwnedEventListener
    SOURCE "${CMAKE_CURRENT_LIST_DIR}/../fixtures/NativeOwnedEventListener.lvui"
    INCLUDES NativeOwnedEventListenerFixture.h OUTPUT_VARIABLE _generated)
add_library(LVUIWebListenerFixture OBJECT "${CMAKE_CURRENT_LIST_DIR}/../native_owned_event_listener_compile.cpp" "${_generated}")
target_include_directories(LVUIWebListenerFixture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../fixtures")
target_link_libraries(LVUIWebListenerFixture PRIVATE LVUI::Native iiGUI::iiGUI)

function(web_component_fixture name fixture translation_unit)
    lvui_generate(NAME "${name}" SOURCE "${CMAKE_CURRENT_LIST_DIR}/../fixtures/${name}.lvui"
        INCLUDES "${fixture}" OUTPUT_VARIABLE generated)
    add_library("${name}WebCompile" OBJECT "${CMAKE_CURRENT_LIST_DIR}/../${translation_unit}" "${generated}")
    target_include_directories("${name}WebCompile" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../fixtures")
    target_link_libraries("${name}WebCompile" PRIVATE LVUI::Native iiGUI::iiGUI)
endfunction()
web_component_fixture(NativeOwnedButton NativeOwnedButtonFixture.h native_owned_button_compile.cpp)
web_component_fixture(NativeOwnedCheckBox NativeOwnedCheckBoxFixture.h native_owned_check_box_compile.cpp)
web_component_fixture(NativeOwnedToggleSwitch NativeOwnedToggleSwitchFixture.h native_owned_toggle_switch_compile.cpp)
