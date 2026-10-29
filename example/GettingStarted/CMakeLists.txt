cmake_minimum_required(VERSION 3.31)
project(LVUIGettingStarted LANGUAGES CXX)

include(CTest)
if(NOT TARGET LVUI::Native)
    find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
endif()
if(NOT TARGET iiGUI::iiGUI)
    find_package(iiGUI 0.4 CONFIG REQUIRED)
endif()
if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Darwin|Windows|Linux)$" OR ANDROID)
    message(FATAL_ERROR "These examples require a desktop iiGUI host")
endif()
if(NOT iiGUI_NATIVE_RESOURCES)
    message(FATAL_ERROR "The iiGUI package must expose its native resource files")
endif()
list(GET iiGUI_NATIVE_RESOURCES 0 _first_native_resource)
get_filename_component(_native_resources_dir "${_first_native_resource}" DIRECTORY)
foreach(_resource IN LISTS iiGUI_NATIVE_RESOURCES)
    get_filename_component(_resource_dir "${_resource}" DIRECTORY)
    if(NOT _resource_dir STREQUAL _native_resources_dir OR NOT EXISTS "${_resource}")
        message(FATAL_ERROR "The iiGUI resource files must exist in one resource directory")
    endif()
endforeach()

function(lvui_tutorial name)
    cmake_parse_arguments(PARSE_ARGV 1 TUTORIAL "" "" "INCLUDES")
    set(include_arguments)
    if(TUTORIAL_INCLUDES)
        list(APPEND include_arguments INCLUDES ${TUTORIAL_INCLUDES})
    endif()
    lvui_generate(NAME "${name}" SOURCE "App/${name}.lvui"
        ${include_arguments} OUTPUT_VARIABLE generated)
    iisdkcore_add_application("LVUI${name}" GUI
        SOURCES App/Application.cpp "${generated}"
        LIBRARIES iiGUI::iiGUI LVUI::Native)
    target_compile_features("LVUI${name}" PRIVATE cxx_std_23)
    target_include_directories("LVUI${name}" PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/App")
    target_compile_definitions("LVUI${name}" PRIVATE
        "LVUI_EXAMPLE_HEADER=\"${name}.generated.h\"" "LVUI_EXAMPLE_VIEW=${name}")
    set_target_properties("LVUI${name}" PROPERTIES CXX_EXTENSIONS OFF
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.lvui.example.${name}"
        MACOSX_BUNDLE_BUNDLE_NAME "LVUI ${name}")
    # Development builds refer to the package's original resources instead of
    # copying them into build/. Distribution packaging must bundle them itself.
    if(APPLE)
        set(_resource_link "$<TARGET_BUNDLE_CONTENT_DIR:LVUI${name}>/Resources")
    else()
        set(_resource_link "$<TARGET_FILE_DIR:LVUI${name}>/resources")
    endif()
    add_custom_command(TARGET "LVUI${name}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E create_symlink "${_native_resources_dir}" "${_resource_link}"
        VERBATIM)
    set("${name}_GENERATED" "${generated}" PARENT_SCOPE)
endfunction()

lvui_tutorial(Hello)
lvui_tutorial(ReadingList INCLUDES ReadingListBackend.h)
lvui_tutorial(Inventory INCLUDES InventoryBackend.h)

# The headless integration test uses iiGUI's internal test host from its source
# checkout; the applications above only consume public installed interfaces.
set(LVUI_TUTORIAL_IIGUI_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../iiGUI/src"
    CACHE PATH "iiGUI source directory used by the optional tutorial tests")
if(BUILD_TESTING AND EXISTS "${LVUI_TUTORIAL_IIGUI_SOURCE}/iiGUI/WidgetState.h")
    add_executable(LVUITutorialTests "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/tutorial_generated.cpp"
        "${Hello_GENERATED}" "${ReadingList_GENERATED}" "${Inventory_GENERATED}")
    target_compile_features(LVUITutorialTests PRIVATE cxx_std_23)
    target_include_directories(LVUITutorialTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/App" "${LVUI_TUTORIAL_IIGUI_SOURCE}")
    target_link_libraries(LVUITutorialTests PRIVATE LVUI::Native iiGUI::iiGUI)
    add_test(NAME LVUI.Tutorial.Generated COMMAND LVUITutorialTests)
    set_tests_properties(LVUI.Tutorial.Generated PROPERTIES TIMEOUT 60)
elseif(BUILD_TESTING)
    message(STATUS "LVUI tutorial test skipped: set LVUI_TUTORIAL_IIGUI_SOURCE to the iiGUI source directory")
endif()
