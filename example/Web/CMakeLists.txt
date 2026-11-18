cmake_minimum_required(VERSION 3.31)
project(LVUIWebExample LANGUAGES CXX)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "The LVUI Web example requires the Emscripten toolchain")
endif()
if(NOT DEFINED CMAKE_CXX_SCAN_FOR_MODULES)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
find_package(iiGUI 0.6 CONFIG REQUIRED)

# The source fonts stay in resources/. The generated package contains the
# Emscripten data archive, and does not need a development resource copy.
set(LVUI_WEB_IIGUI_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../iiGUI/resources/native"
    CACHE PATH "Directory containing the iiGUI native font resources")
set(_resources)
foreach(_name IN LISTS iiGUI_NATIVE_RESOURCE_FILES)
    list(APPEND _resources "${LVUI_WEB_IIGUI_RESOURCE_DIR}/${_name}")
endforeach()

function(lvui_existing_web_view target view source model_dir model_header)
    # Reuse the desktop example or existing component test fixture unchanged.
    lvui_add_application("${target}" NAME "${view}" SOURCE "${source}"
        INCLUDES "${model_header}"
        CONFIGURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/App/Application.cpp")
    target_include_directories("${target}" PRIVATE "${model_dir}")
    target_compile_definitions("${target}" PRIVATE
        "LVUI_EXAMPLE_HEADER=\"${view}.generated.h\"" "LVUI_EXAMPLE_VIEW=${view}")
    lvui_configure_web("${target}"
        SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../resources/platform/web/shell.html"
        RESOURCES ${_resources})
    lvui_install_web("${target}" DESTINATION .)
endfunction()

lvui_existing_web_view(LVUICounterWeb CppViewModelCounter
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/CppViewModelCounter.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted" CppViewModelCounter.h)
lvui_existing_web_view(LVUIReadingListWeb ReadingList
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/App/ReadingList.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/App" ReadingListBackend.h)
lvui_existing_web_view(LVUIInventoryWeb Inventory
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/App/Inventory.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../GettingStarted/App" InventoryBackend.h)
lvui_existing_web_view(LVUIControlsFixtureWeb NativeWindowAccentControls
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/NativeWindowAccentControls.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures" NativeWindowAccentControlsFixture.h)
lvui_existing_web_view(LVUIRadioRowsFixtureWeb NativeOwnedRadioRows
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/NativeOwnedRadioRows.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures" NativeOwnedRadioButtonFixture.h)
lvui_existing_web_view(LVUIToggleFixtureWeb NativeOwnedToggleSwitch
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/NativeOwnedToggleSwitch.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures" NativeOwnedToggleSwitchFixture.h)
lvui_existing_web_view(LVUISliderFixtureWeb NativeOwnedSlider
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/NativeOwnedSlider.lvui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures" NativeOwnedSliderFixture.h)
