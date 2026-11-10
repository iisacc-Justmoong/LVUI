cmake_minimum_required(VERSION 3.31)
project(LVUI VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
option(BUILD_SHARED_LIBS "Build LVUI runtime and compiler libraries as shared libraries" ON)
if(CMAKE_CROSSCOMPILING)
    set(_lvui_default_compiler_tools OFF)
else()
    set(_lvui_default_compiler_tools ON)
endif()
option(LVUI_BUILD_COMPILER_TOOLS "Build compiler command-line tools for the target platform" ${_lvui_default_compiler_tools})
if(BUILD_TESTING AND NOT LVUI_BUILD_COMPILER_TOOLS)
    message(STATUS "LVUI project tests are not registered when target compiler tools are disabled")
endif()
include(cmake/SourceHeaders.cmake)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
file(GLOB LVUI_BUILTIN_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/*.svg")
set(LVUI_BUILTIN_ICON_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons/LVUI/BuiltinIconAssets.h")
add_custom_command(OUTPUT "${LVUI_BUILTIN_ICON_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_icons.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons" --output "${LVUI_BUILTIN_ICON_HEADER}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_icons.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/catalog/lvui-style-tokens.json" ${LVUI_BUILTIN_ICON_FILES}
    VERBATIM)
add_custom_target(LVUIBuiltinIconAssets DEPENDS "${LVUI_BUILTIN_ICON_HEADER}")
file(GLOB LVUI_AUTHORED_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icons/*.svg")
file(GLOB LVUI_DESIGN_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/design-icons/*.svg")
file(GLOB LVUI_AUTHORED_ICON_FALLBACK_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-fallbacks/*.svg")
file(GLOB LVUI_AUTHORED_ICON_REFERENCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-fallbacks/*.png")
set(LVUI_AUTHORED_ICON_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons/AuthoredIconAssets.cpp")
add_custom_command(OUTPUT "${LVUI_AUTHORED_ICON_SOURCE}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_authored_icons.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icons"
        --manifest "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-manifest.json"
        --fallback-input "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-fallbacks"
        --fallback-manifest "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-fallbacks/manifest.json"
        --design-input "${CMAKE_CURRENT_SOURCE_DIR}/resources/design-icons"
        --design-manifest "${CMAKE_CURRENT_SOURCE_DIR}/resources/design-icon-manifest.json"
        --output "${LVUI_AUTHORED_ICON_SOURCE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_authored_icons.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-manifest.json" ${LVUI_AUTHORED_ICON_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/design-icon-manifest.json" ${LVUI_DESIGN_ICON_FILES}
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icon-fallbacks/manifest.json"
        ${LVUI_AUTHORED_ICON_FALLBACK_FILES} ${LVUI_AUTHORED_ICON_REFERENCE_FILES}
    VERBATIM)
set(LVUI_BUILTIN_HUE_RING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons/LVUI/BuiltinHueRing.h")
add_custom_command(OUTPUT "${LVUI_BUILTIN_HUE_RING_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_raster.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/color-picker-hue-ring.png" --output "${LVUI_BUILTIN_HUE_RING_HEADER}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_raster.py" "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/color-picker-hue-ring.png"
    VERBATIM)
add_custom_target(LVUIBuiltinHueRing DEPENDS "${LVUI_BUILTIN_HUE_RING_HEADER}")
find_package(iiSDKCore 0.18.1 CONFIG REQUIRED COMPONENTS Contracts Observation)
include(cmake/LVUICoreObservation.cmake)
add_library(LVUI src/Compiler/Parser.cpp src/Language/Language.cpp src/Compiler/NativeMarkup.cpp src/Compiler/Compiler.cpp src/Compiler/FigmaComponents.cpp src/Language/ConstantExpression.cpp src/Style/StyleTokens.cpp)
add_library(LVUI::LVUI ALIAS LVUI)
target_compile_features(LVUI PUBLIC cxx_std_23)
target_link_libraries(LVUI PUBLIC iiSDKCore::Contracts)
target_include_directories(LVUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUI LVUIBuiltinIconAssets LVUIBuiltinHueRing)
set_target_properties(LVUI PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
add_library(LVUINative src/Runtime/NativeBindings.cpp src/Media/AuthoredIcons.cpp
    "${LVUI_AUTHORED_ICON_SOURCE}")
add_library(LVUI::Native ALIAS LVUINative)
set_target_properties(LVUINative PROPERTIES EXPORT_NAME Native CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_compile_features(LVUINative PUBLIC cxx_std_23)
target_link_libraries(LVUINative PUBLIC iiSDKCore::Observation)
target_include_directories(LVUINative PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUINative LVUIBuiltinIconAssets LVUIBuiltinHueRing)
# Existing generated-view consumers of LVUI::LVUI keep linking after adopting
# native reactive output. Runtime-only consumers can select LVUI::Native.
target_link_libraries(LVUI PUBLIC LVUI::Native)
option(LVUI_BUILD_LOCALIZATION_IO "Build the optional JSON string-catalog IO contract" OFF)
if(LVUI_BUILD_LOCALIZATION_IO)
    add_library(LVUILocalization INTERFACE)
    add_library(LVUI::Localization ALIAS LVUILocalization)
    set_target_properties(LVUILocalization PROPERTIES EXPORT_NAME Localization)
    target_link_libraries(LVUILocalization INTERFACE LVUI::Native)
    install(TARGETS LVUILocalization EXPORT LVUILocalizationTargets)
    install(EXPORT LVUILocalizationTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
endif()
add_library(LVUICompilerBridge SHARED src/Python/CompilerBridge.cpp)
add_library(LVUI::CompilerBridge ALIAS LVUICompilerBridge)
target_compile_features(LVUICompilerBridge PRIVATE cxx_std_23)
target_compile_definitions(LVUICompilerBridge PRIVATE LVUI_COMPILER_BRIDGE_BUILD)
target_link_libraries(LVUICompilerBridge PRIVATE LVUI::LVUI)
target_include_directories(LVUICompilerBridge PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
set_target_properties(LVUICompilerBridge PROPERTIES EXPORT_NAME CompilerBridge CXX_EXTENSIONS OFF
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON VERSION ${PROJECT_VERSION} SOVERSION 1)
if(APPLE)
    target_link_options(LVUICompilerBridge PRIVATE "LINKER:-dead_strip_dylibs")
    set_property(TARGET LVUICompilerBridge APPEND PROPERTY INSTALL_RPATH "@loader_path")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(LVUICompilerBridge PRIVATE "LINKER:--as-needed")
    set_property(TARGET LVUICompilerBridge APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
endif()
option(LVUI_BUILD_COLOR_ADAPTER "Expose the optional color source contract" OFF)
if(LVUI_BUILD_COLOR_ADAPTER)
    add_library(LVUIColor INTERFACE)
    add_library(LVUI::Color ALIAS LVUIColor)
    set_target_properties(LVUIColor PROPERTIES EXPORT_NAME Color)
    target_link_libraries(LVUIColor INTERFACE LVUI::Native)
    install(TARGETS LVUIColor EXPORT LVUIColorTargets)
    install(EXPORT LVUIColorTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
    if(BUILD_TESTING AND LVUI_BUILD_COMPILER_TOOLS)
        # This is an application composition, never an LVUI dependency.
        find_package(iiColorManager 0.1.1 CONFIG QUIET COMPONENTS iiColorManager)
        if(TARGET iiColorManager::iiColorManager)
            add_library(LVUIExampleColorAdapter STATIC example/Integrations/App/ColorSelectionSurface.cpp)
            target_compile_features(LVUIExampleColorAdapter PUBLIC cxx_std_23)
            target_link_libraries(LVUIExampleColorAdapter PUBLIC LVUI::Color PRIVATE iiColorManager::iiColorManager)
        else()
            message(STATUS "Color application composition tests require iiColorManager")
        endif()
    endif()
endif()
option(LVUI_BUILD_RUNTIME "Build the optional activation work runtime" OFF)
if(LVUI_BUILD_RUNTIME)
    find_package(iiSDKCore 0.20 CONFIG REQUIRED COMPONENTS Runtime)
    add_library(LVUIRuntime src/Runtime/NativeWork.cpp)
    add_library(LVUI::Runtime ALIAS LVUIRuntime)
    set_target_properties(LVUIRuntime PROPERTIES EXPORT_NAME Runtime CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_features(LVUIRuntime PUBLIC cxx_std_23)
    target_link_libraries(LVUIRuntime PUBLIC LVUI::Native iiSDKCore::Runtime)
    target_include_directories(LVUIRuntime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    install(TARGETS LVUIRuntime EXPORT LVUIRuntimeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT LVUIRuntimeTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
endif()
set(LVUI_CORE_PROFILE_HINT "" CACHE STRING "Optional relative profile in the sibling iiSDKCore installation")
if(LVUI_CORE_PROFILE_HINT AND (IS_ABSOLUTE "${LVUI_CORE_PROFILE_HINT}" OR
        LVUI_CORE_PROFILE_HINT MATCHES "(^|/)\\.\\.(/|$)"))
    message(FATAL_ERROR "LVUI_CORE_PROFILE_HINT must stay inside the sibling iiSDKCore installation")
endif()
set(_lvui_runtime_targets LVUI LVUINative)
if(TARGET LVUIRuntime)
    list(APPEND _lvui_runtime_targets LVUIRuntime)
endif()
get_target_property(_lvui_observation_type iiSDKCore::Observation TYPE)
if(_lvui_observation_type MATCHES "^(SHARED_LIBRARY|MODULE_LIBRARY|UNKNOWN_LIBRARY)$")
    set(_lvui_core_library_dir "$<TARGET_FILE_DIR:iiSDKCore::Observation>")
endif()
foreach(_target IN LISTS _lvui_runtime_targets)
    set_target_properties(${_target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
    if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set_target_properties(${_target} PROPERTIES FRAMEWORK TRUE FRAMEWORK_VERSION A
            MACOSX_FRAMEWORK_IDENTIFIER "com.iisacc.${_target}")
    elseif(BUILD_SHARED_LIBS AND APPLE)
        if(LVUI_CORE_PROFILE_HINT)
            set_property(TARGET ${_target} APPEND PROPERTY INSTALL_RPATH
                "@loader_path/../../iiSDKCore/${LVUI_CORE_PROFILE_HINT}/${CMAKE_INSTALL_LIBDIR}")
        endif()
        set_property(TARGET ${_target} APPEND PROPERTY INSTALL_RPATH "@loader_path"
            "@loader_path/../../iiSDKCore/${CMAKE_INSTALL_LIBDIR}" ${_lvui_core_library_dir})
    elseif(BUILD_SHARED_LIBS AND UNIX)
        if(LVUI_CORE_PROFILE_HINT)
            set_property(TARGET ${_target} APPEND PROPERTY INSTALL_RPATH
                "$ORIGIN/../../iiSDKCore/${LVUI_CORE_PROFILE_HINT}/${CMAKE_INSTALL_LIBDIR}")
        endif()
        set_property(TARGET ${_target} APPEND PROPERTY INSTALL_RPATH "$ORIGIN"
            "$ORIGIN/../../iiSDKCore/${CMAKE_INSTALL_LIBDIR}" ${_lvui_core_library_dir})
    endif()
endforeach()
option(LVUI_BUILD_IMAGE_IO "Expose the optional image source contract" OFF)
if(LVUI_BUILD_IMAGE_IO)
    add_library(LVUIImageIO INTERFACE)
    add_library(LVUI::ImageIO ALIAS LVUIImageIO)
    set_target_properties(LVUIImageIO PROPERTIES EXPORT_NAME ImageIO)
    target_link_libraries(LVUIImageIO INTERFACE LVUI::Native)
    install(TARGETS LVUIImageIO EXPORT LVUIImageIOTargets)
    install(EXPORT LVUIImageIOTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
    if(BUILD_TESTING AND LVUI_BUILD_COMPILER_TOOLS)
        # An application explicitly composes the independent SDK services.
        find_package(iiSDKCore 0.20 CONFIG QUIET COMPONENTS Runtime)
        find_package(iiGUI 0.5.0 CONFIG QUIET)
        find_package(iiFileProviderNative 0.9 CONFIG QUIET)
        find_package(iiNetwork 0.5 CONFIG QUIET)
        find_package(iiVisualCodec 0.2 CONFIG QUIET COMPONENTS Image)
        find_package(CURL 7.62 QUIET)
        find_package(ZLIB QUIET)
        if(TARGET iiSDKCore::Runtime AND TARGET iiGUI::iiGUI AND TARGET iiFileProvider::Native
           AND TARGET iiNetwork::iiNetwork AND TARGET iiVisualCodec::Image
           AND TARGET CURL::libcurl AND TARGET ZLIB::ZLIB)
            add_library(LVUIExampleImageIOAdapter STATIC example/Integrations/App/NativeImageLoader.cpp example/Integrations/App/NativeImageSource.cpp)
            target_compile_features(LVUIExampleImageIOAdapter PUBLIC cxx_std_23)
            target_include_directories(LVUIExampleImageIOAdapter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/example/Integrations/App")
            target_link_libraries(LVUIExampleImageIOAdapter PUBLIC LVUI::ImageIO iiGUI::iiGUI iiSDKCore::Runtime
                PRIVATE iiFileProvider::Native iiNetwork::iiNetwork iiVisualCodec::Image CURL::libcurl ZLIB::ZLIB)
            add_executable(LVUINativeImageLoaderTests tests/native_image_loader.cpp)
            target_link_libraries(LVUINativeImageLoaderTests PRIVATE LVUIExampleImageIOAdapter iiNetwork::iiNetwork iiVisualCodec::Image)
            target_compile_definitions(LVUINativeImageLoaderTests PRIVATE LVUI_IMAGE_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
            add_test(NAME LVUI.NativeImageLoader.Model COMMAND LVUINativeImageLoaderTests)
            set_tests_properties(LVUI.NativeImageLoader.Model PROPERTIES TIMEOUT 90)
            add_executable(LVUINativeImageAsyncTests tests/native_image_async.cpp)
            target_link_libraries(LVUINativeImageAsyncTests PRIVATE LVUIExampleImageIOAdapter iiNetwork::iiNetwork)
            target_compile_definitions(LVUINativeImageAsyncTests PRIVATE LVUI_IMAGE_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources")
            add_test(NAME LVUI.NativeImageLoader.Async COMMAND LVUINativeImageAsyncTests)
            set_tests_properties(LVUI.NativeImageLoader.Async PROPERTIES TIMEOUT 90)
            foreach(_case IN ITEMS success redirect status empty limit loop gzip_async gzip_limit gzip_http gzip_double gzip_invalid)
                add_test(NAME LVUI.NativeImageLoader.Http.${_case}
                    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_image_http.py" "$<TARGET_FILE:LVUINativeImageLoaderTests>" ${_case})
                set_tests_properties(LVUI.NativeImageLoader.Http.${_case} PROPERTIES TIMEOUT 90)
            endforeach()
        else()
            message(STATUS "Image application composition tests require the chosen image, storage and network SDKs")
        endif()
    endif()
endif()
if(MSVC)
    target_compile_options(LVUI PRIVATE /W4 PUBLIC /Zc:__cplusplus /utf-8)
    target_compile_options(LVUINative PRIVATE /W4 PUBLIC /Zc:__cplusplus /utf-8)
else()
    target_compile_options(LVUI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(LVUINative PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(LVUI_BUILD_COMPILER_TOOLS)
    add_executable(lvui-check src/lvui-check.cpp)
    target_link_libraries(lvui-check PRIVATE LVUI::LVUI)
    set_target_properties(lvui-check PROPERTIES CXX_EXTENSIONS OFF MACOSX_BUNDLE FALSE)
    add_executable(lvui-compile src/lvui-compile.cpp)
    add_executable(LVUI::lvui-compile ALIAS lvui-compile)
    target_link_libraries(lvui-compile PRIVATE LVUI::LVUI)
    set_target_properties(lvui-compile PROPERTIES CXX_EXTENSIONS OFF MACOSX_BUNDLE FALSE)
    # These tools emit text and do not execute native bindings.
    foreach(_tool lvui-check lvui-compile)
        if(APPLE)
            target_link_options(${_tool} PRIVATE "LINKER:-dead_strip_dylibs")
            set_property(TARGET ${_tool} APPEND PROPERTY INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
        elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${_tool} PRIVATE "LINKER:--as-needed")
            set_property(TARGET ${_tool} APPEND PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
        endif()
    endforeach()
endif()
include(cmake/LVUIAot.cmake)
option(LVUI_BUILD_STYLE_EXAMPLE "Build the LVUI style example with an application-supplied iiGUI host" OFF)
if(LVUI_BUILD_STYLE_EXAMPLE)
    add_subdirectory(example/StyleTokens)
endif()
option(LVUI_BUILD_TUTORIAL_EXAMPLES "Build the LVUI getting-started applications" OFF)
if(LVUI_BUILD_TUTORIAL_EXAMPLES)
    add_subdirectory(example/GettingStarted)
endif()
if(BUILD_TESTING AND LVUI_BUILD_COMPILER_TOOLS AND TARGET iiGUI::iiGUI AND TARGET iiSDKCore::Runtime)
    # Compile and link actual app entries; these fixtures never open an OS host.
    lvui_add_application(LVUINativeApplicationFixture
        SOURCE tests/fixtures/NativeApplication.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_application_generated)
    target_include_directories(LVUINativeApplicationFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeApplicationMarkupFixture
        SOURCE tests/fixtures/NativeApplicationMarkup.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeApplicationMarkupFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeForegroundFixture
        SOURCE tests/fixtures/NativeApplicationForeground.lvui INCLUDES NativeForegroundBackend.h
        OUTPUT_VARIABLE _native_foreground_generated)
    target_include_directories(LVUINativeForegroundFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeForegroundMarkupFixture
        SOURCE tests/fixtures/NativeApplicationForegroundMarkup.lvui INCLUDES NativeForegroundBackend.h
        OUTPUT_VARIABLE _native_foreground_markup_generated)
    target_include_directories(LVUINativeForegroundMarkupFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeLaunchFixture
        SOURCE tests/fixtures/NativeLaunchApplication.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_launch_generated)
    target_include_directories(LVUINativeLaunchFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeLaunchMarkupFixture
        SOURCE tests/fixtures/NativeLaunchMarkup.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeLaunchMarkupFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeLaunchScenesFixture
        SOURCE tests/fixtures/NativeLaunchScenes.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeLaunchScenesFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeCloseFixture
        SOURCE tests/fixtures/NativeApplicationClose.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_close_generated)
    target_include_directories(LVUINativeCloseFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeCloseMarkupFixture
        SOURCE tests/fixtures/NativeApplicationCloseMarkup.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeCloseMarkupFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeCloseScenesFixture
        SOURCE tests/fixtures/NativeApplicationCloseScenes.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_close_scenes_generated)
    target_include_directories(LVUINativeCloseScenesFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeWindowCloseFixture
        SOURCE tests/fixtures/NativeWindowClose.lvui
        INCLUDES NativeApplicationBackend.h WindowCloseFixtureBackend.h
        OUTPUT_VARIABLE _native_window_close_generated)
    target_include_directories(LVUINativeWindowCloseFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeSingleWindowCloseFixture
        SOURCE tests/fixtures/NativeSingleWindowClose.lvui
        INCLUDES NativeApplicationBackend.h WindowCloseFixtureBackend.h
        OUTPUT_VARIABLE _native_single_window_close_generated)
    target_include_directories(LVUINativeSingleWindowCloseFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeWindowCloseLaterFixture
        SOURCE tests/fixtures/NativeWindowCloseLater.lvui INCLUDES WindowCloseFixtureBackend.h)
    target_include_directories(LVUINativeWindowCloseLaterFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeQuitOnlyFixture SOURCE tests/fixtures/NativeApplicationQuitOnly.lvui
        OUTPUT_VARIABLE _native_quit_generated)
    lvui_add_application(LVUINativeMultiWindowFixture
        SOURCE tests/fixtures/NativeMultiWindow.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_multi_window_generated)
    target_include_directories(LVUINativeMultiWindowFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeMultiWindowMarkupFixture
        SOURCE tests/fixtures/NativeMultiWindowMarkup.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeMultiWindowMarkupFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeMixedScenesFixture
        SOURCE tests/fixtures/NativeMixedScenes.lvui INCLUDES NativeApplicationBackend.h)
    target_include_directories(LVUINativeMixedScenesFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeNamedWindowsFixture
        SOURCE tests/fixtures/NativeNamedWindows.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_named_windows_generated)
    target_include_directories(LVUINativeNamedWindowsFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeScenesFixture
        SOURCE tests/fixtures/NativeScenes.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_scenes_generated)
    target_include_directories(LVUINativeScenesFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeSingleSceneFixture
        SOURCE tests/fixtures/NativeSingleScene.lvui INCLUDES NativeApplicationBackend.h
        OUTPUT_VARIABLE _native_single_scene_generated)
    target_include_directories(LVUINativeSingleSceneFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUIScenesExampleFixture SOURCE example/GettingStarted/App/Scenes.lvui)
    lvui_add_application(LVUINativeNamedWindowsMarkupFixture
        SOURCE tests/fixtures/NativeNamedWindowsMarkup.lvui)
    lvui_add_application(LVUINativeDynamicWindowsFixture
        SOURCE tests/fixtures/NativeDynamicWindows.lvui INCLUDES NativeDynamicWindowsBackend.h
        OUTPUT_VARIABLE _native_dynamic_windows_generated)
    target_include_directories(LVUINativeDynamicWindowsFixture PRIVATE tests/fixtures)
    lvui_add_application(LVUINativeDynamicWindowsMarkupFixture
        SOURCE tests/fixtures/NativeDynamicWindowsMarkup.lvui INCLUDES NativeDynamicWindowsBackend.h
        OUTPUT_VARIABLE _native_dynamic_windows_markup_generated)
    target_include_directories(LVUINativeDynamicWindowsMarkupFixture PRIVATE tests/fixtures)
endif()
set(_package "${CMAKE_INSTALL_LIBDIR}/cmake/LVUI")
install(TARGETS LVUI LVUINative EXPORT LVUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS LVUICompilerBridge EXPORT LVUITargets LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY src/Python/ii_lvui DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/python
    PATTERN "__pycache__" EXCLUDE)
if(LVUI_BUILD_COMPILER_TOOLS)
    install(TARGETS lvui-check RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(TARGETS lvui-compile EXPORT LVUITargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES
    src/Compiler/Document.h
    src/Python/CompilerBridge.h
    src/Media/AuthoredIcons.h
    src/Media/NativeAuthoredIcons.h
    src/Controls/Card.h
    src/Controls/NativeCard.h
    src/Editing/NativeEditorTimeline.h
    src/Collections/Hierarchy.h
    src/Collections/HierarchyRowContext.h
    src/Collections/List.h
    src/Collections/ListDrag.h
    src/Collections/ListEditing.h
    src/Collections/ListRowContext.h
    src/Collections/ListItem.h
    src/Collections/ListItemMenu.h
    src/Collections/ListItemDetailActions.h
    src/Collections/ListItemDetailControls.h
    src/Collections/ListItemTask.h
    src/Collections/ListItemPreviewRow.h
    src/Collections/ListItemForm.h
    src/Collections/ListItemSegments.h
    src/Collections/NativeListItemSegments.h
    src/Collections/ListItemQuantity.h
    src/Collections/ListItemSelector.h
    src/Collections/NativeListItemSelector.h
    src/Runtime/NativeVisual.h
    src/Collections/NativeListItemDrawing.h
    src/Collections/NativeListItem.h
    src/Collections/NativeListItemComposite.h
    src/Collections/ListViewport.h
    src/Layout/PanelRegions.h
    src/Collections/ListPanel.h
    src/Collections/ListViewValues.h
    src/Collections/NativeListView.h
    src/Collections/NativeListPanel.h
    src/Collections/HierarchyPanel.h
    src/Collections/NativeHierarchyPanel.h
    src/Collections/HierarchyDrag.h
    src/Collections/HierarchyItem.h
    src/Collections/HierarchyItemProjection.h
    src/Collections/HierarchyViewport.h
    src/Collections/ListToolbar.h
    src/Collections/Table.h
    src/Collections/TableRowView.h
    src/Collections/NativeTableRow.h
    src/Collections/TableCellItem.h
    src/Collections/NativeTableCellItem.h
    src/Collections/TableHeader.h
    src/Collections/NativeTableHeaderPresentation.h
    src/Collections/NativeTableHeader.h
    src/Collections/TableClipboard.h
    src/Collections/TableMerge.h
    src/Collections/TableEditing.h
    src/Collections/TableSorting.h
    src/Collections/TableStructure.h
    src/Editing/CodeDocument.h
    src/Editing/CodeViewport.h
    src/Editing/CodeComposition.h
    src/Editing/CodeEditorController.h
    src/Editing/CodeEditorSession.h
    src/Editing/NativeCodeEditor.h
    src/Controls/ToolbarButton.h
    src/Controls/NativeToolbarButton.h
    src/Collections/ListFooter.h
    src/Collections/NativeListFooter.h
    src/Collections/HierarchyToolbar.h
    src/Collections/HierarchyToolbarLayout.h
    src/Collections/NativeHierarchyToolbar.h
    src/Collections/TableAppearance.h
    src/Collections/TableText.h
    src/Collections/TableViewport.h
    src/Collections/TableViewContext.h
    src/Collections/NativeTableViewGroup.h
    src/Collections/NativeTable.h
    src/Collections/NativeListToolbar.h
    src/Collections/NativeHierarchyItem.h
    src/Collections/NativeHierarchyList.h
    src/Language/Language.h
    src/Compiler/Compiler.h
    src/Language/ConstantExpression.h
    src/Runtime/NativeBindings.h
    src/Runtime/NativeInputEvents.h
    src/Runtime/NativeComponentInputs.h
    src/Runtime/NativeObjectInputs.h
    src/Runtime/NativeBackendFactories.h
    src/Runtime/NativeAppStrings.h
    src/Runtime/NativeStringCatalog.h
    src/Runtime/NativeActivation.h
    src/Runtime/ApplicationFileEvents.h
    src/Runtime/ApplicationCommands.h
    src/Runtime/Scene.h
    src/Runtime/SceneRegistry.h
    src/Runtime/SceneEffectGroup.h
    src/Runtime/SceneLifecycle.h
    src/Runtime/SceneStartup.h
    src/Runtime/SceneWindowSet.h
    src/Runtime/WindowCloseGuard.h
    src/Runtime/NativeWork.h
    src/Runtime/NativeValues.h
    src/Runtime/NativeMethods.h
    src/Runtime/NativeQueries.h
    src/Runtime/NativeQueryConnections.h
    src/Runtime/NativeObjects.h
    src/Runtime/NativeInferredValues.h
    src/Style/NativeColors.h
    src/Style/NativeWidgetColors.h
    src/Layout/NativeWidgetLayout.h
    src/Layout/NativeLayoutValues.h
    src/Runtime/NativeWindowValues.h
    src/Style/WindowAccent.h
    src/Runtime/NativeFunctionVariable.h
    src/Collections/NativeCollection.h
    src/Collections/NativeCollectionNavigation.h
    src/Collections/NativeCollectionWidgets.h
    src/Collections/NativeVirtualCollection.h
    src/Editing/NativeLineIndex.h
    src/Collections/NativeMeasuredCollection.h
    src/Style/Motion.h
    src/Media/IconCatalog.h
    src/Media/NativeIconCache.h
    src/Controls/Tab.h
    src/Controls/NativeTab.h
    src/Controls/NativeTabBar.h
    src/Runtime/NativeSection.h
    src/Overlays/Menu.h
    src/Overlays/MenuItem.h
    src/Overlays/NativeMenuItem.h
    src/Overlays/NativeMenu.h
    src/Overlays/Popover.h
    src/Overlays/NativePopover.h
    src/Overlays/Modal.h
    src/Overlays/NativeModal.h
    src/Overlays/NativePopupOpen.h
    src/Overlays/AlertButton.h
    src/Overlays/NativeAlertButton.h
    src/Overlays/Alert.h
    src/Overlays/NativeAlert.h
    src/Color/ColorPickerButton.h
    src/Color/ColorPicker.h
    src/Color/ColorPickerLayout.h
    src/Color/ColorSource.h
    src/Color/ColorSelection.h
    src/Color/ColorSurface.h
    src/Color/ColorChannels.h
    src/Color/ColorChannelSurface.h
    src/Color/NativeColorChannel.h
    src/Color/ColorSelectionSurface.h
    src/Color/ColorPickerSession.h
    src/Color/NativeColorPickerValues.h
    src/Color/NativeColorPicker.h
    src/Color/NativeColorSurface.h
    src/Color/NativeColorPickerButton.h
    src/Controls/Button.h
    src/Controls/NativeButtonMethods.h
    src/Controls/NativeButton.h
    src/Controls/HelpButton.h
    src/Controls/DropdownButton.h
    src/Controls/NativeDropdownButton.h
    src/Controls/ToggleSwitch.h
    src/Controls/NativeToggleSwitch.h
    src/Controls/CheckBox.h
    src/Controls/NativeCheckBox.h
    src/Controls/RadioButton.h
    src/Controls/NativeRadioButton.h
    src/Style/Material.h
    src/Style/NativeMaterialGradient.h
    src/Style/NativeMaterial.h
    src/Navigation/Navigation.h
    src/Navigation/NavigationHistory.h
    src/Navigation/PageRouter.h
    src/Navigation/NativePageRouter.h
    src/Controls/AppCard.h
    src/Controls/NativeAvatarAssets.h
    src/Overlays/Tooltip.h
    src/Overlays/TooltipOutline.h
    src/Overlays/TooltipController.h
    src/Overlays/NativeTooltipPresentation.h
    src/Overlays/NativeTooltipScheduler.h
    src/Overlays/NativeTooltipTarget.h
    src/Overlays/NativeTooltip.h
    src/Controls/Slider.h
    src/Controls/NativeSlider.h
    src/Controls/Stepper.h
    src/Controls/NativeStepper.h
    src/Controls/ComboBox.h
    src/Controls/NativeComboBox.h
    src/Controls/SegmentedControl.h
    src/Controls/NativeSegmentedControl.h
    src/Controls/NativeStylePort.h
    src/Navigation/Route.h
    src/Navigation/MobileNavigation.h
    src/Navigation/MobileNavigationMotion.h
    src/Navigation/NativeMobileNavigation.h
    src/Navigation/NativeMobileTab.h
    src/Controls/Link.h
    src/Controls/NativeLink.h
    src/Media/NativeImageResolver.h
    src/Editing/NativeDocument.h
    src/Controls/Text.h
    src/Editing/TextInput.h
    src/Editing/SurroundingText.h
    src/Editing/TextReplacementCursor.h
    src/Editing/InputField.h
    src/Editing/TextEditor.h
    src/Editing/NativeTextBoundary.h
    src/Editing/NativeInputField.h
    src/Editing/NativeTextEditor.h
    src/Editing/NativeSearchField.h
    src/Editing/NativeTextFieldMaterial.h
    src/Style/SearchFieldTokens.generated.h
    src/Style/MobileNavigationTokens.generated.h
    src/Style/ListItemNavigationTokens.generated.h
    src/Style/SearchFieldMaterial.generated.h
    src/Style/IconButtonDefaults.generated.h
    src/Style/DesignIconDefaults.generated.h
    src/Style/MaterialRadialDefaults.generated.h
    src/Style/ButtonMotionDefaults.generated.h
    src/Style/SegmentedDefaults.generated.h
    src/Style/SliderTicks.generated.h
    src/Style/SliderCenter.generated.h
    src/Style/SliderFilled.generated.h
    src/Style/SliderSegmented.generated.h
    src/Style/SliderSymbol.generated.h
    src/Style/SliderEndpointTokens.generated.h
    src/Style/StyleTokens.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LVUI)
install(FILES "${LVUI_BUILTIN_ICON_HEADER}" "${LVUI_BUILTIN_HUE_RING_HEADER}" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LVUI)
install(DIRECTORY resources/icons DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES example/Integrations/CMakeLists.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/examples/Integrations)
install(FILES example/Integrations/App/ColorSelectionSurface.cpp example/Integrations/App/ColorSurfaceRasterCache.h
    DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/examples/Integrations/App COMPONENT LVUIColorIntegration)
install(FILES example/Integrations/App/NativeImageLoader.cpp example/Integrations/App/NativeImageSource.cpp
    example/Integrations/App/NativeImageAsync.h example/Integrations/App/NativeImageLoader.h DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/examples/Integrations/App)
install(DIRECTORY resources/authored-icons DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(DIRECTORY resources/design-icons DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES resources/design-icon-manifest.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-design-icon-geometry.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-material-radial.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-button-motion.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-slider-center.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-slider-filled.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-slider-labeled-segmented.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-slider-symbol.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES catalog/lvui-segmented-geometry.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(DIRECTORY resources/authored-icon-fallbacks DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources
    FILES_MATCHING PATTERN "*.svg" PATTERN "*.png" PATTERN "manifest.json")
install(FILES resources/authored-icon-manifest.json DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES resources/licenses/LVRS-MIT.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/licenses)
install(FILES resources/images/color-picker-hue-ring.png DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources/images)
install(EXPORT LVUITargets NAMESPACE LVUI:: DESTINATION "${_package}")
configure_package_config_file(cmake/LVUIConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfig.cmake" INSTALL_DESTINATION "${_package}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/LVUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfigVersion.cmake" DESTINATION "${_package}")
install(FILES cmake/LVUIAot.cmake cmake/LVUIInstallApplication.cmake cmake/RunLVUICompiler.cmake cmake/LVUICoreObservation.cmake DESTINATION "${_package}")
install(FILES resources/platform/ios/LVUIInfo.plist.in DESTINATION "${_package}/ios")
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI)
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI PATTERN .DS_Store EXCLUDE)
install(FILES catalog/lvui-keywords.json catalog/lvui-style-tokens.json catalog/figma-lvui-components.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/catalog)
if(BUILD_TESTING AND LVUI_BUILD_COMPILER_TOOLS)
    if(BUILD_SHARED_LIBS AND NOT CMAKE_CROSSCOMPILING)
        set(_lvui_runtime_type)
        if(TARGET LVUIRuntime)
            set(_lvui_runtime_type "-DRUNTIME_TYPE=$<TARGET_PROPERTY:LVUIRuntime,TYPE>")
        endif()
        add_test(NAME LVUI.Shared.TargetTypes COMMAND "${CMAKE_COMMAND}"
            "-DCOMPILER_TYPE=$<TARGET_PROPERTY:LVUI,TYPE>"
            "-DNATIVE_TYPE=$<TARGET_PROPERTY:LVUINative,TYPE>"
            ${_lvui_runtime_type}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/shared_target_types.cmake")
        add_test(NAME LVUI.Shared.CompilerLink COMMAND "${CMAKE_COMMAND}"
            "-DIMAGE=$<TARGET_FILE:lvui-compile>" "-DEXPECTED=$<TARGET_FILE:LVUI>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/shared_link.cmake")
    endif()
    if(WIN32)
        # Public headers must coexist with Windows SDK min/max macros without
        # changing the consuming application's preprocessor environment.
        add_library(LVUIWindowsHeaderMacros OBJECT tests/windows_header_macros.cpp)
        target_link_libraries(LVUIWindowsHeaderMacros PRIVATE LVUI::Native)
        add_test(NAME LVUI.WindowsHeaderMacros.Source
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/windows_header_macros.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()
    set(_lvui_resource_runner "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/resource_runner.py" auto "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_executable(LVUICompilerBridgeTests tests/python_bridge.cpp)
    target_include_directories(LVUICompilerBridgeTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/include")
    target_link_libraries(LVUICompilerBridgeTests PRIVATE LVUICompilerBridge)
    add_test(NAME LVUI.PythonBridge.Native COMMAND LVUICompilerBridgeTests)
    add_test(NAME LVUI.PythonBridge.API COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_python_api.py")
    set_tests_properties(LVUI.PythonBridge.API PROPERTIES
        ENVIRONMENT "LVUI_COMPILER_LIBRARY=$<TARGET_FILE:LVUICompilerBridge>" TIMEOUT 60)
    add_executable(LVUINativeAppStringsTests tests/native_app_strings.cpp)
    target_link_libraries(LVUINativeAppStringsTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Localization.Storage COMMAND LVUINativeAppStringsTests)
    if(BUILD_SHARED_LIBS AND NOT CMAKE_CROSSCOMPILING)
        add_test(NAME LVUI.Shared.NativeLink COMMAND "${CMAKE_COMMAND}"
            "-DIMAGE=$<TARGET_FILE:LVUINativeAppStringsTests>" "-DEXPECTED=$<TARGET_FILE:LVUINative>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/shared_link.cmake")
    endif()
    add_executable(LVUINativeStringCatalogPortTests tests/native_string_catalog_ports.cpp)
    target_link_libraries(LVUINativeStringCatalogPortTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Localization.Ports COMMAND LVUINativeStringCatalogPortTests)
    add_executable(LVUINativeImageResolverContractTests tests/native_image_resolver_contract.cpp)
    target_link_libraries(LVUINativeImageResolverContractTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Image.ResolverContract COMMAND LVUINativeImageResolverContractTests)
    add_test(NAME LVUI.ImageIO.PackageBoundary
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_imageio_package_boundary.py"
                "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Color.PackageBoundary
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_color_package_boundary.py"
                "${CMAKE_CURRENT_SOURCE_DIR}")
    if(LVUI_BUILD_LOCALIZATION_IO)
        # A test application may explicitly compose independent SDKs. These
        # packages are never linked into LVUI or exported to its consumers.
        find_package(iiJson 0.1 CONFIG QUIET)
        find_package(iiFileProviderNative 0.9 CONFIG QUIET)
        if(TARGET iiJson::iiJson AND TARGET iiFileProvider::Native)
            set(LVUI_LOCALIZATION_COMPOSITION_AVAILABLE TRUE)
            add_executable(LVUINativeStringCatalogTests tests/native_string_catalog.cpp)
            target_link_libraries(LVUINativeStringCatalogTests PRIVATE LVUI::Localization iiJson::iiJson iiFileProvider::Native)
            add_test(NAME LVUI.Localization.Catalog COMMAND ${_lvui_resource_runner}
                "$<TARGET_FILE:LVUINativeStringCatalogTests>" "${CMAKE_CURRENT_SOURCE_DIR}/example/GettingStarted/resources/strings" @WORK@)
        else()
            message(STATUS "Localization application composition tests require iiJson and iiFileProviderNative")
        endif()
    endif()
    add_test(NAME LVUI.ResourceWorkspace COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_resource_runner.py")
    add_test(NAME LVUI.WorkspaceValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_workspace.py")
    add_test(NAME LVUI.source_layout COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_layout.cmake)
    add_test(NAME LVUI.Button.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/button_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIButtonCoreBindingTests tests/button_binding_core.cpp)
    target_link_libraries(LVUIButtonCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Button.CoreBinding COMMAND LVUIButtonCoreBindingTests)
    add_test(NAME LVUI.ButtonMethods.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/button_methods_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIButtonMethodsCoreBindingTests tests/button_methods_binding_core.cpp)
    target_link_libraries(LVUIButtonMethodsCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ButtonMethods.CoreBinding COMMAND LVUIButtonMethodsCoreBindingTests)
    add_test(NAME LVUI.ToolbarButton.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/toolbar_button_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.ListFooter.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_footer_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIToolbarFooterCoreBindingTests tests/toolbar_footer_binding_core.cpp)
    target_link_libraries(LVUIToolbarFooterCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ToolbarFooter.CoreBinding COMMAND LVUIToolbarFooterCoreBindingTests)
    add_test(NAME LVUI.ListToolbar.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_toolbar_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListToolbarCoreBindingTests tests/list_toolbar_binding_core.cpp)
    target_link_libraries(LVUIListToolbarCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListToolbar.CoreBinding COMMAND LVUIListToolbarCoreBindingTests)
    add_test(NAME LVUI.Icon.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/icon_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.AuthoredIcon.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/authored_icon_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIAuthoredIconPortTests tests/native_authored_icon_port.cpp)
    target_link_libraries(LVUIAuthoredIconPortTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.AuthoredIcon.Port COMMAND LVUIAuthoredIconPortTests)
    add_test(NAME LVUI.Modal.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/modal_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIModalCoreBindingTests tests/modal_binding_core.cpp)
    target_link_libraries(LVUIModalCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Modal.CoreBinding COMMAND LVUIModalCoreBindingTests)
    add_test(NAME LVUI.ListItem.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.AppCard.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/app_card_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.AppHeader.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/app_header_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.EventListener.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/event_listener_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Alert.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/alert_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIAlertCoreBindingTests tests/alert_binding_core.cpp)
    target_link_libraries(LVUIAlertCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Alert.CoreBinding COMMAND LVUIAlertCoreBindingTests)
    add_test(NAME LVUI.AlertButton.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/alert_button_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIAlertButtonCoreBindingTests tests/alert_button_binding_core.cpp)
    target_link_libraries(LVUIAlertButtonCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.AlertButton.CoreBinding COMMAND LVUIAlertButtonCoreBindingTests)
    add_executable(LVUIOwnedIconCatalogTests tests/owned_icon_catalog.cpp)
    target_link_libraries(LVUIOwnedIconCatalogTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedIcons.Catalog COMMAND LVUIOwnedIconCatalogTests)
    add_executable(LVUIAuthoredIconCatalogTests tests/authored_icon_catalog.cpp)
    target_link_libraries(LVUIAuthoredIconCatalogTests PRIVATE LVUI::Native)
    target_compile_definitions(LVUIAuthoredIconCatalogTests PRIVATE
        LVUI_AUTHORED_ICON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/authored-icons")
    add_test(NAME LVUI.AuthoredIcons.Catalog COMMAND LVUIAuthoredIconCatalogTests)
    add_test(NAME LVUI.AuthoredIcons.Embedding COMMAND ${_lvui_resource_runner}
        "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_authored_icon_embedding.py")
    add_executable(LVUIDesignIconCatalogTests tests/design_icon_catalog.cpp)
    target_link_libraries(LVUIDesignIconCatalogTests PRIVATE LVUI::Native)
    target_compile_definitions(LVUIDesignIconCatalogTests PRIVATE
        LVUI_DESIGN_ICON_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources/design-icons")
    add_test(NAME LVUI.DesignIcons.Catalog COMMAND LVUIDesignIconCatalogTests)
    add_test(NAME LVUI.DesignIcons.Embedding COMMAND ${_lvui_resource_runner}
        "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_design_icon_embedding.py")
    add_test(NAME LVUI.OwnedIcons.Embedding COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_icon_embedding.py")
    add_test(NAME LVUI.OwnedColorButton.Embedding COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_raster_embedding.py")
    add_executable(LVUIOwnedColorButtonTests tests/owned_color_picker_button.cpp)
    target_link_libraries(LVUIOwnedColorButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorButton.Model COMMAND LVUIOwnedColorButtonTests)
    add_executable(LVUIOwnedColorSelectionTests tests/owned_color_selection.cpp)
    target_link_libraries(LVUIOwnedColorSelectionTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorSelection.Model COMMAND LVUIOwnedColorSelectionTests)
    add_executable(LVUIOwnedColorPickerTests tests/owned_color_picker.cpp)
    target_link_libraries(LVUIOwnedColorPickerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorPicker.Model COMMAND LVUIOwnedColorPickerTests)
    add_executable(LVUIListToolbarCompileTests tests/list_toolbar_codegen.cpp)
    target_link_libraries(LVUIListToolbarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListToolbar.Codegen COMMAND LVUIListToolbarCompileTests)
    add_executable(LVUIOwnedListToolbarTests tests/owned_list_toolbar.cpp)
    target_link_libraries(LVUIOwnedListToolbarTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListToolbar.Values COMMAND LVUIOwnedListToolbarTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedHierarchyDragTests tests/owned_hierarchy_drag.cpp)
    target_link_libraries(LVUIOwnedHierarchyDragTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedHierarchy.Drag COMMAND LVUIOwnedHierarchyDragTests)
    endif()
    add_executable(LVUIOwnedListItemActionsCompileTests tests/list_item_actions_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemActionsCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.ActionsCodegen COMMAND LVUIOwnedListItemActionsCompileTests)
    add_executable(LVUIOwnedListItemStepperCompileTests tests/list_item_stepper_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemStepperCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.StepperCodegen COMMAND LVUIOwnedListItemStepperCompileTests)
    add_executable(LVUIOwnedListItemSelectCompileTests tests/list_item_select_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemSelectCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.SelectCodegen COMMAND LVUIOwnedListItemSelectCompileTests)
    add_executable(LVUIOwnedListItemInlineEditCompileTests tests/list_item_inline_edit_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemInlineEditCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.InlineEditCodegen COMMAND LVUIOwnedListItemInlineEditCompileTests)
    add_executable(LVUIOwnedListItemCompositeCompileTests tests/list_item_composite_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemCompositeCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.CompositeCodegen COMMAND LVUIOwnedListItemCompositeCompileTests)
    add_executable(LVUIOwnedListItemCompileTests tests/list_item_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Codegen COMMAND LVUIOwnedListItemCompileTests)
    add_executable(LVUIOwnedCardCompileTests tests/card_codegen.cpp)
    target_link_libraries(LVUIOwnedCardCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCard.Codegen COMMAND LVUIOwnedCardCompileTests)
    add_executable(LVUICardOpenCompileTests tests/card_open_codegen.cpp)
    target_link_libraries(LVUICardOpenCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.CardOpen.Codegen COMMAND LVUICardOpenCompileTests)
    add_executable(LVUIOwnedTableHeaderCompileTests tests/table_header_codegen.cpp)
    target_link_libraries(LVUIOwnedTableHeaderCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableHeader.Codegen COMMAND LVUIOwnedTableHeaderCompileTests)
    add_test(NAME LVUI.ColorPicker.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_picker_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorPickerHostCoreBindingTests tests/color_picker_host_binding_core.cpp)
    target_link_libraries(LVUIColorPickerHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorPicker.HostCoreBinding COMMAND LVUIColorPickerHostCoreBindingTests)
    add_test(NAME LVUI.ColorPicker.LayoutBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_picker_layout_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorPickerLayoutCoreBindingTests tests/color_picker_layout_binding_core.cpp)
    target_link_libraries(LVUIColorPickerLayoutCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorPicker.LayoutCoreBinding COMMAND LVUIColorPickerLayoutCoreBindingTests)
    add_test(NAME LVUI.ColorChannel.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_channel_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorChannelHostCoreBindingTests tests/color_channel_host_binding_core.cpp)
    target_link_libraries(LVUIColorChannelHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorChannel.HostCoreBinding COMMAND LVUIColorChannelHostCoreBindingTests)
    add_test(NAME LVUI.ColorPicker.ValuesBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_picker_values_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorPickerValuesCoreBindingTests tests/color_picker_values_binding_core.cpp)
    target_link_libraries(LVUIColorPickerValuesCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorPicker.ValuesCoreBinding COMMAND LVUIColorPickerValuesCoreBindingTests)
    add_test(NAME LVUI.TableRow.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_row_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableRowHostCoreBindingTests tests/table_row_host_binding_core.cpp)
    target_link_libraries(LVUITableRowHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableRow.HostCoreBinding COMMAND LVUITableRowHostCoreBindingTests)
    add_test(NAME LVUI.TableCell.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_cell_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableCellModelCoreBindingTests tests/table_cell_model_binding_core.cpp)
    target_link_libraries(LVUITableCellModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableCell.ModelCoreBinding COMMAND LVUITableCellModelCoreBindingTests)
    add_test(NAME LVUI.TableCell.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_cell_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableCellHostCoreBindingTests tests/table_cell_host_binding_core.cpp)
    target_link_libraries(LVUITableCellHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableCell.HostCoreBinding COMMAND LVUITableCellHostCoreBindingTests)
    add_executable(LVUIOwnedTableCellItemCompileTests tests/table_cell_item_codegen.cpp)
    target_link_libraries(LVUIOwnedTableCellItemCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableCellItem.Codegen COMMAND LVUIOwnedTableCellItemCompileTests)
    add_executable(LVUIOwnedTableRowCompileTests tests/table_row_codegen.cpp)
    target_link_libraries(LVUIOwnedTableRowCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableRow.Codegen COMMAND LVUIOwnedTableRowCompileTests)
    add_executable(LVUIOwnedTableRowTests tests/owned_table_row_view.cpp)
    target_link_libraries(LVUIOwnedTableRowTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableRow.Model COMMAND LVUIOwnedTableRowTests)
    add_executable(LVUIOwnedTableCellItemTests tests/owned_table_cell_item.cpp)
    target_link_libraries(LVUIOwnedTableCellItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableCellItem.Model COMMAND LVUIOwnedTableCellItemTests)
    add_test(NAME LVUI.TableHeader.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_header_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableHeaderHostCoreBindingTests tests/table_header_host_binding_core.cpp)
    target_link_libraries(LVUITableHeaderHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableHeader.HostCoreBinding COMMAND LVUITableHeaderHostCoreBindingTests)
    add_test(NAME LVUI.TableHeader.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_header_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableHeaderModelCoreBindingTests tests/table_header_model_binding_core.cpp)
    target_link_libraries(LVUITableHeaderModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableHeader.ModelCoreBinding COMMAND LVUITableHeaderModelCoreBindingTests)
    add_executable(LVUIOwnedTableHeaderTests tests/owned_table_header.cpp)
    target_link_libraries(LVUIOwnedTableHeaderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableHeader.Model COMMAND LVUIOwnedTableHeaderTests)
    add_executable(LVUIOwnedCardTests tests/owned_card.cpp)
    target_link_libraries(LVUIOwnedCardTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCard.Model COMMAND LVUIOwnedCardTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemQuantityTests tests/owned_list_item_quantity.cpp)
    target_link_libraries(LVUIOwnedListItemQuantityTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.Quantity COMMAND LVUIOwnedListItemQuantityTests)
    endif()
    add_test(NAME LVUI.ColorChannelSurface.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_channel_surface_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorChannelSurfaceCoreBindingTests tests/color_channel_surface_binding_core.cpp)
    target_link_libraries(LVUIColorChannelSurfaceCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorChannelSurface.CoreBinding COMMAND LVUIColorChannelSurfaceCoreBindingTests)
    add_test(NAME LVUI.ListItemSelector.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_selector_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListItemSelectorCoreBindingTests tests/list_item_selector_binding_core.cpp)
    target_link_libraries(LVUIListItemSelectorCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListItemSelector.CoreBinding COMMAND LVUIListItemSelectorCoreBindingTests)
    add_executable(LVUIOwnedListItemSelectorTests tests/owned_list_item_selector.cpp)
    target_link_libraries(LVUIOwnedListItemSelectorTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Selector COMMAND LVUIOwnedListItemSelectorTests)
    add_executable(LVUIOwnedListItemTests tests/owned_list_item.cpp)
    target_link_libraries(LVUIOwnedListItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Values COMMAND LVUIOwnedListItemTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemMenuTests tests/owned_list_item_menu.cpp)
    target_link_libraries(LVUIOwnedListItemMenuTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.MenuModel COMMAND LVUIOwnedListItemMenuTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemDetailActionsTests tests/owned_list_item_detail_actions.cpp)
    target_link_libraries(LVUIOwnedListItemDetailActionsTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.DetailActionsModel COMMAND LVUIOwnedListItemDetailActionsTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemDetailControlsTests tests/owned_list_item_detail_controls.cpp)
    target_link_libraries(LVUIOwnedListItemDetailControlsTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.DetailControlsModel COMMAND LVUIOwnedListItemDetailControlsTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemTaskTests tests/owned_list_item_task.cpp)
    target_link_libraries(LVUIOwnedListItemTaskTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.TaskModel COMMAND LVUIOwnedListItemTaskTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemPreviewRowTests tests/owned_list_item_preview_row.cpp)
    target_link_libraries(LVUIOwnedListItemPreviewRowTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.PreviewRowModel COMMAND LVUIOwnedListItemPreviewRowTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListItemFormTests tests/owned_list_item_form.cpp)
    target_link_libraries(LVUIOwnedListItemFormTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedListItem.FormModel COMMAND LVUIOwnedListItemFormTests)
    endif()
    add_executable(LVUIOwnedListItemFormCompileTests tests/list_item_form_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemFormCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.FormCodegen COMMAND LVUIOwnedListItemFormCompileTests)
    add_executable(LVUIOwnedListItemPreviewCompileTests tests/list_item_preview_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemPreviewCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.PreviewCodegen COMMAND LVUIOwnedListItemPreviewCompileTests)
    add_executable(LVUIOwnedListItemTaskCompileTests tests/list_item_task_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemTaskCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.TaskCodegen COMMAND LVUIOwnedListItemTaskCompileTests)
    add_test(NAME LVUI.ListItemSegments.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_segments_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListItemSegmentsCoreBindingTests tests/list_item_segments_binding_core.cpp)
    target_link_libraries(LVUIListItemSegmentsCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListItemSegments.CoreBinding COMMAND LVUIListItemSegmentsCoreBindingTests)
    add_executable(LVUIOwnedListItemSegmentsTests tests/owned_list_item_segments.cpp)
    target_link_libraries(LVUIOwnedListItemSegmentsTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.SegmentsModel COMMAND LVUIOwnedListItemSegmentsTests)
    add_executable(LVUIOwnedListItemDetailControlsCompileTests tests/list_item_detail_controls_codegen.cpp)
    target_link_libraries(LVUIOwnedListItemDetailControlsCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.DetailControlsCodegen COMMAND LVUIOwnedListItemDetailControlsCompileTests)
    add_executable(LVUIOwnedListModelTests tests/owned_list_model.cpp)
    target_link_libraries(LVUIOwnedListModelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.Model COMMAND LVUIOwnedListModelTests)
    add_executable(LVUIOwnedListEditingTests tests/owned_list_editing.cpp)
    target_link_libraries(LVUIOwnedListEditingTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.Editing COMMAND LVUIOwnedListEditingTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListDragTests tests/owned_list_drag.cpp)
    target_link_libraries(LVUIOwnedListDragTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedList.Drag COMMAND LVUIOwnedListDragTests)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedListViewportTests tests/owned_list_viewport.cpp)
    target_link_libraries(LVUIOwnedListViewportTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedList.Viewport COMMAND LVUIOwnedListViewportTests)
    endif()
    add_executable(LVUIOwnedListPanelTests tests/owned_list_panel.cpp)
    target_link_libraries(LVUIOwnedListPanelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.PanelLayout COMMAND LVUIOwnedListPanelTests)
    add_executable(LVUIOwnedListPanelCompileTests tests/list_panel_codegen.cpp)
    target_link_libraries(LVUIOwnedListPanelCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.Codegen COMMAND LVUIOwnedListPanelCompileTests)
    add_executable(LVUIOwnedHierarchyPanelTests tests/owned_hierarchy_panel.cpp)
    target_link_libraries(LVUIOwnedHierarchyPanelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyPanel.Layout COMMAND LVUIOwnedHierarchyPanelTests)
    add_executable(LVUIOwnedHierarchyPanelCompileTests tests/hierarchy_panel_codegen.cpp)
    target_link_libraries(LVUIOwnedHierarchyPanelCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyPanel.Codegen COMMAND LVUIOwnedHierarchyPanelCompileTests)
    add_executable(LVUITableCompileTests tests/table_codegen.cpp)
    target_link_libraries(LVUITableCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Codegen COMMAND LVUITableCompileTests)
    add_executable(LVUIOwnedTableEditingTests tests/owned_table_editing.cpp)
    target_link_libraries(LVUIOwnedTableEditingTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Editing COMMAND LVUIOwnedTableEditingTests)
    add_executable(LVUIOwnedTableClipboardTests tests/owned_table_clipboard.cpp)
    target_link_libraries(LVUIOwnedTableClipboardTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Clipboard COMMAND LVUIOwnedTableClipboardTests)
    add_executable(LVUIOwnedTableSortingTests tests/owned_table_sorting.cpp)
    target_link_libraries(LVUIOwnedTableSortingTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Sorting COMMAND LVUIOwnedTableSortingTests)
    add_executable(LVUIOwnedTableStructureTests tests/owned_table_structure.cpp)
    target_link_libraries(LVUIOwnedTableStructureTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Structure COMMAND LVUIOwnedTableStructureTests)
    add_executable(LVUIOwnedTableStructureActionsTests tests/owned_table_structure_actions.cpp)
    target_link_libraries(LVUIOwnedTableStructureActionsTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.StructureActions COMMAND LVUIOwnedTableStructureActionsTests)
    add_executable(LVUIOwnedTableMergeTests tests/owned_table_merge.cpp)
    target_link_libraries(LVUIOwnedTableMergeTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Merge COMMAND LVUIOwnedTableMergeTests)
    add_executable(LVUIOwnedCodeDocumentTests tests/owned_code_document.cpp)
    target_link_libraries(LVUIOwnedCodeDocumentTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Model COMMAND LVUIOwnedCodeDocumentTests)
    add_executable(LVUIOwnedCodeEditorControllerTests tests/owned_code_editor_controller.cpp)
    target_link_libraries(LVUIOwnedCodeEditorControllerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Controller COMMAND LVUIOwnedCodeEditorControllerTests)
    if(TARGET iiGUI::iiGUI)
        add_executable(LVUIOwnedCodeViewportTests tests/owned_code_viewport.cpp)
        target_link_libraries(LVUIOwnedCodeViewportTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
        add_test(NAME LVUI.OwnedCodeDocument.Viewport COMMAND LVUIOwnedCodeViewportTests)
    endif()
    add_executable(LVUIOwnedCodeCompositionTests tests/owned_code_composition.cpp)
    target_link_libraries(LVUIOwnedCodeCompositionTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Composition COMMAND LVUIOwnedCodeCompositionTests)
    add_executable(LVUIOwnedCodeEditorSessionTests tests/owned_code_editor_session.cpp)
    target_link_libraries(LVUIOwnedCodeEditorSessionTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Session COMMAND LVUIOwnedCodeEditorSessionTests)
    add_executable(LVUIOwnedCodeEditorCompileTests tests/code_editor_codegen.cpp)
    target_link_libraries(LVUIOwnedCodeEditorCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Codegen COMMAND LVUIOwnedCodeEditorCompileTests)
    add_executable(LVUIOwnedToolbarButtonTests tests/owned_toolbar_button.cpp)
    target_link_libraries(LVUIOwnedToolbarButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToolbarButton.Values COMMAND LVUIOwnedToolbarButtonTests)
    add_executable(LVUIOwnedToolbarButtonCompileTests tests/toolbar_button_codegen.cpp)
    target_link_libraries(LVUIOwnedToolbarButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToolbarButton.Codegen COMMAND LVUIOwnedToolbarButtonCompileTests)
    add_executable(LVUIOwnedListFooterTests tests/owned_list_footer.cpp)
    target_link_libraries(LVUIOwnedListFooterTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListFooter.Values COMMAND LVUIOwnedListFooterTests)
    add_executable(LVUIOwnedListFooterCompileTests tests/list_footer_codegen.cpp)
    target_link_libraries(LVUIOwnedListFooterCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListFooter.Codegen COMMAND LVUIOwnedListFooterCompileTests)
    add_test(NAME LVUI.CodeEditor.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/code_editor_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUICodeEditorHostCoreBindingTests tests/code_editor_host_binding_core.cpp)
    target_link_libraries(LVUICodeEditorHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.CodeEditor.HostCoreBinding COMMAND LVUICodeEditorHostCoreBindingTests)
    add_test(NAME LVUI.ListView.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_view_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListViewHostCoreBindingTests tests/list_view_host_binding_core.cpp)
    target_link_libraries(LVUIListViewHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListView.HostCoreBinding COMMAND LVUIListViewHostCoreBindingTests)
    add_test(NAME LVUI.ListPanel.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_panel_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListPanelHostCoreBindingTests tests/list_panel_host_binding_core.cpp)
    target_link_libraries(LVUIListPanelHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListPanel.HostCoreBinding COMMAND LVUIListPanelHostCoreBindingTests)
    add_test(NAME LVUI.HierarchyPanel.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_panel_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyPanelHostCoreBindingTests tests/hierarchy_panel_host_binding_core.cpp)
    target_link_libraries(LVUIHierarchyPanelHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyPanel.HostCoreBinding COMMAND LVUIHierarchyPanelHostCoreBindingTests)
    add_test(NAME LVUI.PanelLayout.Boundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/panel_layout_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIPanelLayoutCoreTests tests/panel_layout_core.cpp)
    target_link_libraries(LVUIPanelLayoutCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.PanelLayout.CoreBinding COMMAND LVUIPanelLayoutCoreTests)
    add_test(NAME LVUI.HierarchyList.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_list_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyListHostCoreBindingTests tests/hierarchy_list_host_binding_core.cpp)
    target_link_libraries(LVUIHierarchyListHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyList.HostCoreBinding COMMAND LVUIHierarchyListHostCoreBindingTests)
    add_test(NAME LVUI.HierarchyToolbar.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_toolbar_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyToolbarHostCoreBindingTests tests/hierarchy_toolbar_host_binding_core.cpp)
    target_link_libraries(LVUIHierarchyToolbarHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyToolbar.HostCoreBinding COMMAND LVUIHierarchyToolbarHostCoreBindingTests)
    add_test(NAME LVUI.HierarchyToolbar.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_toolbar_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyToolbarModelCoreBindingTests tests/hierarchy_toolbar_model_binding_core.cpp)
    target_link_libraries(LVUIHierarchyToolbarModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyToolbar.ModelCoreBinding COMMAND LVUIHierarchyToolbarModelCoreBindingTests)
    add_executable(LVUIOwnedHierarchyToolbarTests tests/owned_hierarchy_toolbar.cpp)
    target_link_libraries(LVUIOwnedHierarchyToolbarTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyToolbar.Model COMMAND LVUIOwnedHierarchyToolbarTests)
    add_executable(LVUIOwnedHierarchyToolbarCompileTests tests/hierarchy_toolbar_codegen.cpp)
    target_link_libraries(LVUIOwnedHierarchyToolbarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyToolbar.Codegen COMMAND LVUIOwnedHierarchyToolbarCompileTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedTableViewportTests tests/owned_table_viewport.cpp)
    target_link_libraries(LVUIOwnedTableViewportTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedTable.Viewport COMMAND LVUIOwnedTableViewportTests)
    endif()
    add_executable(LVUIOwnedTableTests tests/owned_table.cpp)
    target_link_libraries(LVUIOwnedTableTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Model COMMAND LVUIOwnedTableTests)
    add_executable(LVUIOwnedHierarchyTests tests/owned_hierarchy.cpp)
    target_link_libraries(LVUIOwnedHierarchyTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchy.Model COMMAND LVUIOwnedHierarchyTests)
    add_test(NAME LVUI.HierarchyItem.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_item_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyItemModelCoreBindingTests tests/hierarchy_item_model_binding_core.cpp)
    target_link_libraries(LVUIHierarchyItemModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyItem.ModelCoreBinding COMMAND LVUIHierarchyItemModelCoreBindingTests)
    add_test(NAME LVUI.HierarchyItem.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_item_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIHierarchyItemHostCoreBindingTests tests/hierarchy_item_host_binding_core.cpp)
    target_link_libraries(LVUIHierarchyItemHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.HierarchyItem.HostCoreBinding COMMAND LVUIHierarchyItemHostCoreBindingTests)
    add_executable(LVUIOwnedHierarchyItemTests tests/owned_hierarchy_item.cpp)
    target_link_libraries(LVUIOwnedHierarchyItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyItem.Model COMMAND LVUIOwnedHierarchyItemTests)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUIOwnedHierarchyViewportTests tests/owned_hierarchy_viewport.cpp)
    target_link_libraries(LVUIOwnedHierarchyViewportTests PRIVATE LVUI::LVUI iiGUI::iiGUI)
    add_test(NAME LVUI.OwnedHierarchy.Viewport COMMAND LVUIOwnedHierarchyViewportTests)
    endif()
    add_executable(LVUIHierarchyListCompileTests tests/hierarchy_list_codegen.cpp)
    target_link_libraries(LVUIHierarchyListCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyList.Codegen COMMAND LVUIHierarchyListCompileTests)
    add_executable(LVUIHierarchyItemCompileTests tests/hierarchy_item_codegen.cpp)
    target_link_libraries(LVUIHierarchyItemCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyItem.Codegen COMMAND LVUIHierarchyItemCompileTests)
    add_executable(LVUIOwnedColorPickerLayoutTests tests/owned_color_picker_layout.cpp)
    target_link_libraries(LVUIOwnedColorPickerLayoutTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorPicker.Layout COMMAND LVUIOwnedColorPickerLayoutTests)
    if(TARGET LVUIExampleColorAdapter)
        add_executable(LVUIColorSurfaceRasterCacheTests tests/color_surface_raster_cache.cpp)
        target_link_libraries(LVUIColorSurfaceRasterCacheTests PRIVATE LVUIExampleColorAdapter iiColorManager::iiColorManager)
        add_test(NAME LVUI.ColorSurfaceRasterCache COMMAND LVUIColorSurfaceRasterCacheTests)
        add_executable(LVUIOwnedColorSurfaceTests tests/owned_color_surface.cpp)
        target_link_libraries(LVUIOwnedColorSurfaceTests PRIVATE LVUIExampleColorAdapter)
        add_test(NAME LVUI.OwnedColorSurface.Model COMMAND LVUIOwnedColorSurfaceTests)
        add_executable(LVUIOwnedColorPickerSessionTests tests/owned_color_picker_session.cpp)
        target_link_libraries(LVUIOwnedColorPickerSessionTests PRIVATE LVUIExampleColorAdapter)
        add_test(NAME LVUI.OwnedColorPicker.Session COMMAND LVUIOwnedColorPickerSessionTests)
    endif()
    add_executable(LVUIColorButtonCompileTests tests/color_picker_button_codegen.cpp)
    target_link_libraries(LVUIColorButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorButton.Codegen COMMAND LVUIColorButtonCompileTests)
    add_executable(LVUIColorPickerCompileTests tests/color_picker_codegen.cpp)
    target_link_libraries(LVUIColorPickerCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorPicker.Codegen COMMAND LVUIColorPickerCompileTests)
    add_executable(LVUIColorSurfaceCompileTests tests/color_surface_codegen.cpp)
    target_link_libraries(LVUIColorSurfaceCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorSurface.Codegen COMMAND LVUIColorSurfaceCompileTests)
    add_executable(LVUIOwnedTabTests tests/owned_tab.cpp)
    target_link_libraries(LVUIOwnedTabTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTab.Model COMMAND LVUIOwnedTabTests)
    add_executable(LVUITabCompileTests tests/tabs.cpp)
    target_link_libraries(LVUITabCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTab.Codegen COMMAND LVUITabCompileTests)
    add_executable(LVUITabBarCompileTests tests/tab_bar.cpp)
    target_link_libraries(LVUITabBarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTabBar.Codegen COMMAND LVUITabBarCompileTests)
    add_executable(LVUIOwnedMenuTests tests/owned_menu.cpp)
    target_link_libraries(LVUIOwnedMenuTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMenu.Model COMMAND LVUIOwnedMenuTests)
    add_executable(LVUIMenuCompileTests tests/menu.cpp)
    target_link_libraries(LVUIMenuCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMenu.Codegen COMMAND LVUIMenuCompileTests)
    add_executable(LVUIOwnedPopoverTests tests/owned_popover.cpp)
    target_link_libraries(LVUIOwnedPopoverTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedPopover.Model COMMAND LVUIOwnedPopoverTests)
    add_executable(LVUIOwnedModalTests tests/owned_modal.cpp)
    target_link_libraries(LVUIOwnedModalTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedModal.Values COMMAND LVUIOwnedModalTests)
    add_executable(LVUIOwnedAlertButtonTests tests/owned_alert_button.cpp)
    target_link_libraries(LVUIOwnedAlertButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlertButton.Values COMMAND LVUIOwnedAlertButtonTests)
    add_executable(LVUIOwnedAlertTests tests/owned_alert.cpp)
    target_link_libraries(LVUIOwnedAlertTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlert.Values COMMAND LVUIOwnedAlertTests)
    add_executable(LVUISheetCompileTests tests/sheet_codegen.cpp)
    target_link_libraries(LVUISheetCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSheet.Codegen COMMAND LVUISheetCompileTests)
    add_executable(LVUIAlertCompileTests tests/alert_codegen.cpp)
    target_link_libraries(LVUIAlertCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlert.Codegen COMMAND LVUIAlertCompileTests)
    add_executable(LVUIAlertButtonCompileTests tests/alert_button_codegen.cpp)
    target_link_libraries(LVUIAlertButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlertButton.Codegen COMMAND LVUIAlertButtonCompileTests)
    add_executable(LVUIModalCompileTests tests/modal_codegen.cpp)
    target_link_libraries(LVUIModalCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedModal.Codegen COMMAND LVUIModalCompileTests)
    add_executable(LVUIPopoverCompileTests tests/popover_codegen.cpp)
    target_link_libraries(LVUIPopoverCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedPopover.Codegen COMMAND LVUIPopoverCompileTests)
    add_executable(LVUIOwnedMenuItemTests tests/owned_menu_item.cpp)
    target_link_libraries(LVUIOwnedMenuItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMenuItem.Model COMMAND LVUIOwnedMenuItemTests)
    add_executable(LVUIMenuItemCompileTests tests/menu_item.cpp)
    target_link_libraries(LVUIMenuItemCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMenuItem.Codegen COMMAND LVUIMenuItemCompileTests)
    add_executable(LVUIMotionTests tests/motion.cpp)
    target_compile_features(LVUIMotionTests PRIVATE cxx_std_23)
    target_include_directories(LVUIMotionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/include")
    add_test(NAME LVUI.Motion.Model COMMAND LVUIMotionTests)
    add_executable(LVUIOwnedProgressCodegenTests tests/progress_bar_codegen.cpp)
    target_link_libraries(LVUIOwnedProgressCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedProgress.Codegen COMMAND LVUIOwnedProgressCodegenTests)
    add_executable(LVUINativeCoreValueTests tests/native_core_values.cpp)
    target_link_libraries(LVUINativeCoreValueTests PRIVATE LVUI::Native)
    foreach(_case text numeric keys binding containers dynamic_keys temporary nested_binding named named_temporary named_cpp named_binding owned_reads)
        add_test(NAME LVUI.NativeCoreValues.${_case} COMMAND LVUINativeCoreValueTests ${_case})
    endforeach()
    add_executable(LVUIOpacityTests tests/opacity.cpp)
    target_link_libraries(LVUIOpacityTests PRIVATE LVUI::LVUI)
    foreach(_case values reactive shared fixed components framework delegate diagnostics)
        add_test(NAME LVUI.Opacity.${_case} COMMAND LVUIOpacityTests ${_case})
        set_tests_properties(LVUI.Opacity.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeInferredValueTests tests/native_inferred_values.cpp)
    target_link_libraries(LVUINativeInferredValueTests PRIVATE LVUI::Native)
    foreach(_case scalar methods factories aliases observation reentry maps)
        add_test(NAME LVUI.NativeInferredValues.${_case} COMMAND LVUINativeInferredValueTests ${_case})
        set_tests_properties(LVUI.NativeInferredValues.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeFactoryCaptureTests tests/native_factory_captures.cpp)
    target_link_libraries(LVUINativeFactoryCaptureTests PRIVATE LVUI::Native)
    foreach(_case receiver temporary argument adaptive owned failures frozen categories retirement threads)
        add_test(NAME LVUI.NativeFactoryCaptures.${_case} COMMAND LVUINativeFactoryCaptureTests ${_case})
        set_tests_properties(LVUI.NativeFactoryCaptures.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIImageCompileTests tests/images.cpp)
    add_executable(LVUIOwnedButtonTests tests/owned_button.cpp)
    add_test(NAME LVUI.ButtonDropdown.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/button_dropdown_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(LVUIOwnedButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedButton.Model COMMAND LVUIOwnedButtonTests)
    add_executable(LVUIToggleCoreBindingTests tests/toggle_binding_core.cpp)
    target_link_libraries(LVUIToggleCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ToggleSwitch.CoreBinding COMMAND LVUIToggleCoreBindingTests)
    add_executable(LVUIOwnedToggleSwitchTests tests/owned_toggle_switch.cpp)
    add_test(NAME LVUI.ToggleSwitch.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/toggle_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(LVUIOwnedToggleSwitchTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToggleSwitch.Model COMMAND LVUIOwnedToggleSwitchTests)
    add_test(NAME LVUI.CheckBox.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_box_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUICheckBoxCoreBindingTests tests/check_box_binding_core.cpp)
    target_link_libraries(LVUICheckBoxCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.CheckBox.CoreBinding COMMAND LVUICheckBoxCoreBindingTests)
    add_executable(LVUIOwnedCheckBoxTests tests/owned_check_box.cpp)
    target_link_libraries(LVUIOwnedCheckBoxTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCheckBox.Model COMMAND LVUIOwnedCheckBoxTests)
    add_executable(LVUICheckBoxCompileTests tests/check_box.cpp)
    target_link_libraries(LVUICheckBoxCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCheckBox.Codegen COMMAND LVUICheckBoxCompileTests)
    add_test(NAME LVUI.RadioButton.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/radio_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIRadioCoreBindingTests tests/radio_binding_core.cpp)
    target_link_libraries(LVUIRadioCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.RadioButton.CoreBinding COMMAND LVUIRadioCoreBindingTests)
    add_executable(LVUIOwnedRadioButtonTests tests/owned_radio_button.cpp)
    target_link_libraries(LVUIOwnedRadioButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedRadioButton.Model COMMAND LVUIOwnedRadioButtonTests)
    add_executable(LVUIRadioButtonCompileTests tests/radio_button.cpp)
    target_link_libraries(LVUIRadioButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedRadioButton.Codegen COMMAND LVUIRadioButtonCompileTests)
    add_executable(LVUIOwnedMaterialTests tests/owned_material.cpp)
    target_link_libraries(LVUIOwnedMaterialTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMaterial.Model COMMAND LVUIOwnedMaterialTests)
    add_executable(LVUIMaterialCompileTests tests/material.cpp)
    target_link_libraries(LVUIMaterialCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedMaterial.Codegen COMMAND LVUIMaterialCompileTests)
    add_executable(LVUIEditorTimelinePaletteTests tests/editor_timeline_palette.cpp)
    target_link_libraries(LVUIEditorTimelinePaletteTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.EditorTimeline.Palette COMMAND LVUIEditorTimelinePaletteTests)
    add_test(NAME LVUI.Card.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/card_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUICardCoreBindingTests tests/card_binding_core.cpp)
    target_link_libraries(LVUICardCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Card.CoreBinding COMMAND LVUICardCoreBindingTests)
    add_test(NAME LVUI.TabBar.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tab_bar_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITabBarCoreBindingTests tests/tab_bar_binding_core.cpp)
    target_link_libraries(LVUITabBarCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TabBar.CoreBinding COMMAND LVUITabBarCoreBindingTests)
    add_test(NAME LVUI.Tab.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tab_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITabCoreBindingTests tests/tab_binding_core.cpp)
    target_link_libraries(LVUITabCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tab.CoreBinding COMMAND LVUITabCoreBindingTests)
    add_test(NAME LVUI.ColorSurface.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_surface_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorSurfaceCoreBindingTests tests/color_surface_binding_core.cpp)
    target_link_libraries(LVUIColorSurfaceCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorSurface.CoreBinding COMMAND LVUIColorSurfaceCoreBindingTests)
    add_test(NAME LVUI.CollectionWidget.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/collection_widget_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUICollectionWidgetCoreBindingTests tests/collection_widget_binding_core.cpp)
    target_link_libraries(LVUICollectionWidgetCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.CollectionWidget.CoreBinding COMMAND LVUICollectionWidgetCoreBindingTests)
    add_test(NAME LVUI.Document.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/document_layout_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.EditorTimeline.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_timeline_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.EditorCanvas.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/editor_canvas_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Link.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/link_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUILinkCoreBindingTests tests/link_binding_core.cpp)
    target_link_libraries(LVUILinkCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Link.CoreBinding COMMAND LVUILinkCoreBindingTests)
    add_executable(LVUIOwnedLinkTests tests/owned_link.cpp)
    target_link_libraries(LVUIOwnedLinkTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedLink.Model COMMAND LVUIOwnedLinkTests)
    add_executable(LVUILinkCompileTests tests/link.cpp)
    target_link_libraries(LVUILinkCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedLink.Codegen COMMAND LVUILinkCompileTests)
    add_executable(LVUIOwnedRouteTests tests/owned_routes.cpp)
    target_link_libraries(LVUIOwnedRouteTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedRoute.Model COMMAND LVUIOwnedRouteTests)
    add_executable(LVUIOwnedNavigationHistoryTests tests/owned_navigation_history.cpp)
    target_link_libraries(LVUIOwnedNavigationHistoryTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedNavigationHistory.Model COMMAND LVUIOwnedNavigationHistoryTests)
    add_executable(LVUIOwnedPageRouterTests tests/owned_page_router.cpp)
    target_link_libraries(LVUIOwnedPageRouterTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedPageRouter.Model COMMAND LVUIOwnedPageRouterTests)
    add_executable(LVUIPageRouterCompileTests tests/page_router.cpp)
    target_link_libraries(LVUIPageRouterCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedPageRouter.Codegen COMMAND LVUIPageRouterCompileTests)
    add_executable(LVUIOwnedAppCardTests tests/owned_app_card.cpp)
    target_link_libraries(LVUIOwnedAppCardTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAppCard.Model COMMAND LVUIOwnedAppCardTests)
    add_executable(LVUIAppCardCompileTests tests/app_card.cpp)
    target_link_libraries(LVUIAppCardCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAppCard.Codegen COMMAND LVUIAppCardCompileTests)
    add_executable(LVUIAppHeaderCompileTests tests/app_header.cpp)
    target_link_libraries(LVUIAppHeaderCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAppHeader.Codegen COMMAND LVUIAppHeaderCompileTests)
    add_executable(LVUIEventListenerCompileTests tests/event_listener.cpp)
    target_link_libraries(LVUIEventListenerCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedEventListener.Codegen COMMAND LVUIEventListenerCompileTests)
    add_executable(LVUIAvatarCompileTests tests/avatar.cpp)
    target_link_libraries(LVUIAvatarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAvatar.Codegen COMMAND LVUIAvatarCompileTests)
    add_test(NAME LVUI.Collection.ViewportBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/virtual_viewport_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Collection.NavigationBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/collection_navigation_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Collection.PlannerBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/virtual_collection_planner_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.CodeEditor.ViewportBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/code_viewport_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIFocusRingCodegenTests tests/focus_ring_codegen.cpp)
    target_link_libraries(LVUIFocusRingCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.FocusRing.Codegen COMMAND LVUIFocusRingCodegenTests)
    add_executable(LVUIAbstractInputBarCodegenTests tests/abstract_input_bar_codegen.cpp)
    target_link_libraries(LVUIAbstractInputBarCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.AbstractInputBar.Codegen COMMAND LVUIAbstractInputBarCodegenTests)
    add_executable(LVUIMobileTabCodegenTests tests/mobile_tab_codegen.cpp)
    target_link_libraries(LVUIMobileTabCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.MobileTab.Codegen COMMAND LVUIMobileTabCodegenTests)
    add_executable(LVUIWindowNoiseCodegenTests tests/window_noise_codegen.cpp)
    target_link_libraries(LVUIWindowNoiseCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.WindowNoise.Codegen COMMAND LVUIWindowNoiseCodegenTests)
    add_executable(LVUIWheelGuardCodegenTests tests/wheel_scroll_guard_codegen.cpp)
    target_link_libraries(LVUIWheelGuardCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.WheelGuard.Codegen COMMAND LVUIWheelGuardCodegenTests)
    add_test(NAME LVUI.ListItem.MenuBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_menu_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Table.ViewportBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_viewport_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Hierarchy.ViewportDragBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/hierarchy_viewport_drag_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.ListDrag.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_drag_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.ListViewport.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_viewport_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.ListItem.LayoutBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_layout_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.ListItem.CompositeHostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_composite_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIListItemCompositeHostCoreTests tests/list_item_composite_host_core.cpp)
    target_link_libraries(LVUIListItemCompositeHostCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListItem.CompositeHostCoreBinding COMMAND LVUIListItemCompositeHostCoreTests)
    add_test(NAME LVUI.Table.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableHostCoreTests tests/table_host_core.cpp)
    target_link_libraries(LVUITableHostCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Table.HostCoreBinding COMMAND LVUITableHostCoreTests)
    add_test(NAME LVUI.Tooltip.FacadeBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooltip_facade_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITooltipFacadeCoreTests tests/tooltip_facade_core.cpp)
    target_link_libraries(LVUITooltipFacadeCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tooltip.FacadeCoreBinding COMMAND LVUITooltipFacadeCoreTests)
    add_test(NAME LVUI.Tooltip.TargetHostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooltip_target_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITooltipTargetHostCoreTests tests/tooltip_target_core.cpp)
    target_link_libraries(LVUITooltipTargetHostCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tooltip.TargetHostCoreBinding COMMAND LVUITooltipTargetHostCoreTests)
    add_test(NAME LVUI.Tooltip.PresentationHostBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooltip_presentation_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITooltipPresentationHostCoreTests tests/tooltip_presentation_core.cpp)
    target_link_libraries(LVUITooltipPresentationHostCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tooltip.PresentationHostCoreBinding COMMAND LVUITooltipPresentationHostCoreTests)
    add_test(NAME LVUI.Tooltip.OutlineBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooltip_outline_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITooltipOutlineCoreTests tests/tooltip_outline_core.cpp)
    target_link_libraries(LVUITooltipOutlineCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tooltip.OutlineCoreBinding COMMAND LVUITooltipOutlineCoreTests)
    add_test(NAME LVUI.Tooltip.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/tooltip_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITooltipModelCoreTests tests/tooltip_model_core.cpp)
    target_link_libraries(LVUITooltipModelCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Tooltip.ModelCoreBinding COMMAND LVUITooltipModelCoreTests)
    add_executable(LVUIOwnedTooltipTests tests/owned_tooltip.cpp)
    target_link_libraries(LVUIOwnedTooltipTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Model COMMAND LVUIOwnedTooltipTests)
    add_executable(LVUIOwnedTooltipControllerTests tests/owned_tooltip_controller.cpp)
    target_link_libraries(LVUIOwnedTooltipControllerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Controller COMMAND LVUIOwnedTooltipControllerTests)
    add_executable(LVUITooltipCompileTests tests/tooltip.cpp)
    target_link_libraries(LVUITooltipCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Codegen COMMAND LVUITooltipCompileTests)
    add_test(NAME LVUI.Slider.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/slider_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUISliderHostCoreBindingTests tests/slider_host_binding_core.cpp)
    target_link_libraries(LVUISliderHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Slider.HostCoreBinding COMMAND LVUISliderHostCoreBindingTests)
    add_test(NAME LVUI.Slider.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/slider_model_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUISliderModelCoreBindingTests tests/slider_model_binding_core.cpp)
    target_link_libraries(LVUISliderModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Slider.ModelCoreBinding COMMAND LVUISliderModelCoreBindingTests)
    add_executable(LVUIOwnedSliderTests tests/owned_slider.cpp)
    target_link_libraries(LVUIOwnedSliderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSlider.Model COMMAND LVUIOwnedSliderTests)
    add_executable(LVUIFilledSliderTests tests/filled_slider.cpp)
    target_link_libraries(LVUIFilledSliderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSlider.FilledModel COMMAND LVUIFilledSliderTests)
    add_test(NAME LVUI.TextInput.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/text_input_model_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITextInputCoreBindingTests tests/text_input_model_binding_core.cpp)
    target_link_libraries(LVUITextInputCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TextInput.CoreBinding COMMAND LVUITextInputCoreBindingTests)
    add_executable(LVUIOwnedTextInputTests tests/owned_text_input.cpp)
    target_link_libraries(LVUIOwnedTextInputTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTextInput.Model COMMAND LVUIOwnedTextInputTests)
    add_test(NAME LVUI.InputField.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/input_field_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIInputFieldCoreBindingTests tests/input_field_binding_core.cpp)
    target_link_libraries(LVUIInputFieldCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.InputField.CoreBinding COMMAND LVUIInputFieldCoreBindingTests)
    add_executable(LVUIOwnedInputFieldTests tests/owned_input_field.cpp)
    target_link_libraries(LVUIOwnedInputFieldTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedInputField.Configuration COMMAND LVUIOwnedInputFieldTests)
    add_executable(LVUIInputFieldCompileTests tests/input_field.cpp)
    target_link_libraries(LVUIInputFieldCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedInputField.Codegen COMMAND LVUIInputFieldCompileTests)
    add_test(NAME LVUI.TextEditor.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/text_editor_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITextEditorCoreBindingTests tests/text_editor_binding_core.cpp)
    target_link_libraries(LVUITextEditorCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TextEditor.CoreBinding COMMAND LVUITextEditorCoreBindingTests)
    add_executable(LVUITextEditorCompileTests tests/text_editor_codegen.cpp)
    target_link_libraries(LVUITextEditorCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.TextEditor.Codegen COMMAND LVUITextEditorCompileTests)
    add_executable(LVUITextEditorConfigurationTests tests/text_editor_configuration.cpp)
    target_link_libraries(LVUITextEditorConfigurationTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.TextEditor.Configuration COMMAND LVUITextEditorConfigurationTests)
    add_test(NAME LVUI.MobileNavigation.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/mobile_navigation_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMobileNavigationHostCoreBindingTests tests/mobile_navigation_host_binding_core.cpp)
    target_link_libraries(LVUIMobileNavigationHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.MobileNavigation.HostCoreBinding COMMAND LVUIMobileNavigationHostCoreBindingTests)
    add_test(NAME LVUI.MobileNavigation.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/mobile_navigation_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMobileNavigationModelCoreBindingTests tests/mobile_navigation_model_binding_core.cpp)
    target_link_libraries(LVUIMobileNavigationModelCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.MobileNavigation.ModelCoreBinding COMMAND LVUIMobileNavigationModelCoreBindingTests)
    add_executable(LVUIMobileNavigationCompileTests tests/mobile_navigation_codegen.cpp)
    target_link_libraries(LVUIMobileNavigationCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.MobileNavigation.Codegen COMMAND LVUIMobileNavigationCompileTests)
    add_executable(LVUIMobileNavigationModelTests tests/mobile_navigation_model.cpp)
    target_link_libraries(LVUIMobileNavigationModelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.MobileNavigation.Model COMMAND LVUIMobileNavigationModelTests)
    add_executable(LVUIAccountButtonCompileTests tests/account_button_codegen.cpp)
    target_link_libraries(LVUIAccountButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.AccountButton.Codegen COMMAND LVUIAccountButtonCompileTests)
    add_executable(LVUILabelPresentationCompileTests tests/label_presentation_codegen.cpp)
    target_link_libraries(LVUILabelPresentationCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.LabelPresentation.Codegen COMMAND LVUILabelPresentationCompileTests)
    add_executable(LVUIListItemValueCompileTests tests/list_item_value_codegen.cpp)
    target_link_libraries(LVUIListItemValueCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.ListItemValue.Codegen COMMAND LVUIListItemValueCompileTests)
    add_executable(LVUIListItemValueModelTests tests/list_item_value_model.cpp)
    target_link_libraries(LVUIListItemValueModelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.ListItemValue.Values COMMAND LVUIListItemValueModelTests)
    add_executable(LVUIListItemDetailCompileTests tests/list_item_detail_codegen.cpp)
    target_link_libraries(LVUIListItemDetailCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.ListItemDetail.Codegen COMMAND LVUIListItemDetailCompileTests)
    add_executable(LVUIListItemDetailModelTests tests/list_item_detail_model.cpp)
    target_link_libraries(LVUIListItemDetailModelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.ListItemDetail.Values COMMAND LVUIListItemDetailModelTests)
    add_executable(LVUIMobileNavigationEffectsTests tests/mobile_navigation_effects.cpp)
    target_link_libraries(LVUIMobileNavigationEffectsTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.MobileNavigation.Effects COMMAND LVUIMobileNavigationEffectsTests)
    add_test(NAME LVUI.SearchField.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/search_field_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUISearchFieldHostCoreBindingTests tests/search_field_host_binding_core.cpp)
    target_link_libraries(LVUISearchFieldHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SearchField.HostCoreBinding COMMAND LVUISearchFieldHostCoreBindingTests)
    add_executable(LVUISearchFieldCompileTests tests/search_field_codegen.cpp)
    target_link_libraries(LVUISearchFieldCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSearchField.Codegen COMMAND LVUISearchFieldCompileTests)
    add_executable(LVUIOwnedStepperTests tests/owned_stepper.cpp)
    target_link_libraries(LVUIOwnedStepperTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedStepper.Model COMMAND LVUIOwnedStepperTests)
    add_test(NAME LVUI.MenuItem.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_item_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMenuItemCoreBindingTests tests/menu_item_binding_core.cpp)
    target_link_libraries(LVUIMenuItemCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.MenuItem.CoreBinding COMMAND LVUIMenuItemCoreBindingTests)
    add_test(NAME LVUI.ComboBox.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/combo_box_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIComboBoxCoreBindingTests tests/combo_box_binding_core.cpp)
    target_link_libraries(LVUIComboBoxCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ComboBox.CoreBinding COMMAND LVUIComboBoxCoreBindingTests)
    add_test(NAME LVUI.ColorButton.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/color_button_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIColorButtonCoreBindingTests tests/color_button_binding_core.cpp)
    target_link_libraries(LVUIColorButtonCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ColorButton.CoreBinding COMMAND LVUIColorButtonCoreBindingTests)
    add_test(NAME LVUI.PageRouter.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/page_router_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIPageRouterCoreBindingTests tests/page_router_binding_core.cpp)
    target_link_libraries(LVUIPageRouterCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.PageRouter.CoreBinding COMMAND LVUIPageRouterCoreBindingTests)
    add_test(NAME LVUI.Popover.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/popover_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIPopoverCoreBindingTests tests/popover_binding_core.cpp)
    target_link_libraries(LVUIPopoverCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Popover.CoreBinding COMMAND LVUIPopoverCoreBindingTests)
    add_test(NAME LVUI.Material.HostBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/material_host_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMaterialHostCoreBindingTests tests/material_host_binding_core.cpp)
    target_link_libraries(LVUIMaterialHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Material.CoreHostBinding COMMAND LVUIMaterialHostCoreBindingTests)
    add_test(NAME LVUI.Material.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/material_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.MaterialGradient.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/material_gradient_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMaterialGradientCoreBindingTests tests/material_gradient_binding_core.cpp)
    target_link_libraries(LVUIMaterialGradientCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.MaterialGradient.CoreBinding COMMAND LVUIMaterialGradientCoreBindingTests)
    add_test(NAME LVUI.Menu.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIMenuCoreBindingTests tests/menu_binding_core.cpp)
    target_link_libraries(LVUIMenuCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Menu.CoreBinding COMMAND LVUIMenuCoreBindingTests)
    add_test(NAME LVUI.Menu.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.MenuItem.ModelBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/menu_item_model_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.Stepper.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/stepper_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUIStepperDropdownCoreBindingTests tests/stepper_dropdown_binding_core.cpp)
    target_link_libraries(LVUIStepperDropdownCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.StepperDropdown.CoreBinding COMMAND LVUIStepperDropdownCoreBindingTests)
    add_executable(LVUIStepperCompileTests tests/stepper.cpp)
    target_link_libraries(LVUIStepperCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedStepper.Codegen COMMAND LVUIStepperCompileTests)
    add_executable(LVUIOwnedComboBoxTests tests/owned_combo_box.cpp)
    target_link_libraries(LVUIOwnedComboBoxTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedComboBox.Model COMMAND LVUIOwnedComboBoxTests)
    add_executable(LVUIComboBoxCompileTests tests/combo_box.cpp)
    target_link_libraries(LVUIComboBoxCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedComboBox.Codegen COMMAND LVUIComboBoxCompileTests)
    add_executable(LVUIOwnedSegmentedControlTests tests/owned_segmented_control.cpp)
    target_link_libraries(LVUIOwnedSegmentedControlTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSegmentedControl.Model COMMAND LVUIOwnedSegmentedControlTests)
    add_executable(LVUISegmentedBindingCoreTests tests/segmented_binding_core.cpp)
    target_link_libraries(LVUISegmentedBindingCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SegmentedControl.CoreBinding COMMAND LVUISegmentedBindingCoreTests)
    add_test(NAME LVUI.SegmentedControl.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/segmented_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUISegmentedControlCompileTests tests/segmented_control.cpp)
    target_link_libraries(LVUISegmentedControlCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSegmentedControl.Codegen COMMAND LVUISegmentedControlCompileTests)
    add_executable(LVUIOwnedHelpButtonTests tests/owned_help_button.cpp)
    target_link_libraries(LVUIOwnedHelpButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHelpButton.Model COMMAND LVUIOwnedHelpButtonTests)
    add_executable(LVUIHelpButtonCompileTests tests/help_button.cpp)
    target_link_libraries(LVUIHelpButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHelpButton.Codegen COMMAND LVUIHelpButtonCompileTests)
    add_executable(LVUISceneStartupTests tests/native_scene_startup.cpp)
    target_link_libraries(LVUISceneStartupTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SceneStartup COMMAND LVUISceneStartupTests)
    add_executable(LVUIApplicationFileEventsTests tests/native_application_file_events.cpp)
    target_link_libraries(LVUIApplicationFileEventsTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ApplicationFileEvents COMMAND LVUIApplicationFileEventsTests)
    add_executable(LVUIApplicationCommandsTests tests/native_application_commands.cpp)
    target_link_libraries(LVUIApplicationCommandsTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ApplicationCommands COMMAND LVUIApplicationCommandsTests)
    add_executable(LVUISceneRegistryTests tests/native_scene_registry.cpp)
    target_link_libraries(LVUISceneRegistryTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SceneRegistry COMMAND LVUISceneRegistryTests)
    add_executable(LVUISceneWindowSetTests tests/native_scene_window_set.cpp)
    target_link_libraries(LVUISceneWindowSetTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SceneWindowSet COMMAND LVUISceneWindowSetTests)
    add_executable(LVUISceneLifecycleTests tests/native_scene_lifecycle.cpp)
    target_link_libraries(LVUISceneLifecycleTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.SceneLifecycle COMMAND LVUISceneLifecycleTests)
    add_executable(LVUIOwnedDropdownButtonTests tests/owned_dropdown_button.cpp)
    target_link_libraries(LVUIOwnedDropdownButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedDropdownButton.Model COMMAND LVUIOwnedDropdownButtonTests)
    add_executable(LVUIDropdownButtonCompileTests tests/dropdown_button.cpp)
    target_link_libraries(LVUIDropdownButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedDropdownButton.Codegen COMMAND LVUIDropdownButtonCompileTests)
    add_executable(LVUISliderCompileTests tests/slider.cpp)
    target_link_libraries(LVUISliderCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSlider.Codegen COMMAND LVUISliderCompileTests)
    add_executable(LVUIToggleSwitchCompileTests tests/toggle_switch.cpp)
    target_link_libraries(LVUIToggleSwitchCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToggleSwitch.Codegen COMMAND LVUIToggleSwitchCompileTests)
    add_executable(LVUIDividerCompileTests tests/dividers.cpp)
    target_link_libraries(LVUIDividerCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedDivider.Codegen COMMAND LVUIDividerCompileTests)
    add_executable(LVUIButtonCompileTests tests/buttons.cpp)
    target_link_libraries(LVUIButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedButton.Codegen COMMAND LVUIButtonCompileTests)
    add_executable(LVUINativeViewCompileTests tests/native_views.cpp)
    target_link_libraries(LVUINativeViewCompileTests PRIVATE LVUI::LVUI)
    foreach(_case frameworks bindings empty isolation roots selector diagnostics)
        add_test(NAME LVUI.NativeView.${_case} COMMAND LVUINativeViewCompileTests ${_case})
        set_tests_properties(LVUI.NativeView.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    target_link_libraries(LVUIImageCompileTests PRIVATE LVUI::LVUI)
    foreach(_case shape bindings empty diagnostics)
        add_test(NAME LVUI.Image.${_case} COMMAND LVUIImageCompileTests ${_case})
        set_tests_properties(LVUI.Image.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(LVUIFactoryCaptureTests tests/factory_captures.cpp)
    target_link_libraries(LVUIFactoryCaptureTests PRIVATE LVUI::LVUI)
    foreach(_case receivers arguments typed)
        add_test(NAME LVUI.FactoryCaptures.${_case} COMMAND LVUIFactoryCaptureTests ${_case})
        set_tests_properties(LVUI.FactoryCaptures.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeCallableFieldTests tests/native_callable_fields.cpp)
    target_link_libraries(LVUINativeCallableFieldTests PRIVATE LVUI::Native)
    foreach(_case versions errors initial publication inflight borrowed handles accessors retirement threads)
        add_test(NAME LVUI.NativeCallableFields.${_case} COMMAND LVUINativeCallableFieldTests ${_case})
        set_tests_properties(LVUI.NativeCallableFields.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeMethodReferenceTests tests/native_method_references.cpp)
    target_link_libraries(LVUINativeMethodReferenceTests PRIVATE LVUI::Native)
    foreach(_case lvalues selection failures initial inflight parent temporary retirement threads)
        add_test(NAME LVUI.NativeMethodReferences.${_case} COMMAND LVUINativeMethodReferenceTests ${_case})
        set_tests_properties(LVUI.NativeMethodReferences.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIObjectMethodReferenceTests tests/object_method_references.cpp)
    target_link_libraries(LVUIObjectMethodReferenceTests PRIVATE LVUI::LVUI)
    set_target_properties(LVUIObjectMethodReferenceTests PROPERTIES CXX_EXTENSIONS OFF)
    foreach(_case receivers postfix typed factories diagnostics)
        add_test(NAME LVUI.ObjectMethodReferences.${_case} COMMAND LVUIObjectMethodReferenceTests ${_case})
        set_tests_properties(LVUI.ObjectMethodReferences.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeMethodTests tests/native_methods.cpp)
    target_link_libraries(LVUINativeMethodTests PRIVATE LVUI::Native)
    foreach(_case private failures canonical reentry initial isolation callable_objects)
        add_test(NAME LVUI.NativeMethods.${_case} COMMAND LVUINativeMethodTests ${_case})
        set_tests_properties(LVUI.NativeMethods.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIMethodQueryTests tests/method_queries.cpp)
    target_link_libraries(LVUIMethodQueryTests PRIVATE LVUI::LVUI)
    foreach(_case receivers postfix tracking diagnostics)
        add_test(NAME LVUI.MethodQueries.${_case} COMMAND LVUIMethodQueryTests ${_case})
        set_tests_properties(LVUI.MethodQueries.${_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(LVUIObjectInputTests tests/object_inputs.cpp)
    target_link_libraries(LVUIObjectInputTests PRIVATE LVUI::LVUI)
    foreach(_case values forwarding roots diagnostics)
        add_test(NAME LVUI.ObjectInputs.${_case} COMMAND LVUIObjectInputTests ${_case})
        set_tests_properties(LVUI.ObjectInputs.${_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(LVUINativeObjectInputTests tests/native_object_inputs.cpp)
    target_link_libraries(LVUINativeObjectInputTests PRIVATE LVUI::Native)
    foreach(_case failures ownership handles)
        add_test(NAME LVUI.NativeObjectInputs.${_case} COMMAND LVUINativeObjectInputTests ${_case})
        set_tests_properties(LVUI.NativeObjectInputs.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIWindowTests tests/window.cpp)
    target_link_libraries(LVUIWindowTests PRIVATE LVUI::LVUI)
    foreach(_case dynamic accent constants diagnostics)
        add_test(NAME LVUI.Window.${_case} COMMAND LVUIWindowTests ${_case})
        set_tests_properties(LVUI.Window.${_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(LVUIColorTests tests/colors.cpp)
    add_executable(LVUILayoutTests tests/layout.cpp)
    target_link_libraries(LVUILayoutTests PRIVATE LVUI::LVUI)
    foreach(_case dynamic alignment constants diagnostics)
        add_test(NAME LVUI.Layout.${_case} COMMAND LVUILayoutTests ${_case})
        set_tests_properties(LVUI.Layout.${_case} PROPERTIES TIMEOUT 20)
    endforeach()
    target_link_libraries(LVUIColorTests PRIVATE LVUI::LVUI)
    foreach(_case inputs state capture diagnostics)
        add_test(NAME LVUI.Colors.${_case} COMMAND LVUIColorTests ${_case})
    endforeach()
    add_executable(LVUINativeColorTests tests/native_colors.cpp)
    target_link_libraries(LVUINativeColorTests PRIVATE LVUI::Native)
    foreach(_case values bounds reactive callbacks lifetime threading)
        add_test(NAME LVUI.NativeColors.${_case} COMMAND LVUINativeColorTests ${_case})
    endforeach()
    add_executable(LVUIIndependentColorValuesTests tests/native_color_values_independent.cpp)
    target_compile_features(LVUIIndependentColorValuesTests PRIVATE cxx_std_23)
    add_test(NAME LVUI.NativeColors.IndependentValues COMMAND LVUIIndependentColorValuesTests)
    if(LVUI_BUILD_RUNTIME)
        add_executable(LVUINativeWorkTests tests/native_work.cpp)
        target_link_libraries(LVUINativeWorkTests PRIVATE LVUI::Runtime)
        foreach(_case queued active delivery errors bounds routing parallel reentrant)
            add_test(NAME LVUI.NativeWork.${_case} COMMAND LVUINativeWorkTests ${_case})
            set_tests_properties(LVUI.NativeWork.${_case} PROPERTIES TIMEOUT 20)
        endforeach()
    endif()
    add_executable(LVUIRepetitionTests tests/repetition.cpp)
    target_link_libraries(LVUIRepetitionTests PRIVATE LVUI::LVUI)
    foreach(_case syntax scope projection diagnostics navigation)
        add_test(NAME LVUI.Repetition.${_case} COMMAND LVUIRepetitionTests ${_case})
        set_tests_properties(LVUI.Repetition.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_test(NAME LVUI.Repetition.cli_root COMMAND lvui-check "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/RepeatedRoot.lvui")
    set_tests_properties(LVUI.Repetition.cli_root PROPERTIES PASS_REGULAR_EXPRESSION "root for" TIMEOUT 15)
    add_executable(LVUINativeCollectionTests tests/native_collection.cpp)
    target_link_libraries(LVUINativeCollectionTests PRIVATE LVUI::Native)
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUINativeCollectionNavigationTests tests/native_collection_navigation.cpp)
    target_link_libraries(LVUINativeCollectionNavigationTests PRIVATE LVUI::Native iiGUI::iiGUI)
    foreach(_case navigation selection projection eligibility lifetime modes restore)
        add_test(NAME LVUI.NativeCollectionNavigation.${_case} COMMAND LVUINativeCollectionNavigationTests ${_case})
        set_tests_properties(LVUI.NativeCollectionNavigation.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUINativeVirtualCollectionTests tests/native_virtual_collection.cpp)
    target_link_libraries(LVUINativeVirtualCollectionTests PRIVATE LVUI::Native iiGUI::iiGUI)
    endif()
    if(TARGET iiGUI::iiGUI)
    add_executable(LVUINativeMeasuredCollectionTests tests/native_measured_collection.cpp)
    target_link_libraries(LVUINativeMeasuredCollectionTests PRIVATE LVUI::Native iiGUI::iiGUI)
    foreach(_case index measured grid anchor lifetime diagnostics ambient sparse)
        add_test(NAME LVUI.NativeMeasuredCollection.${_case} COMMAND LVUINativeMeasuredCollectionTests ${_case})
        set_tests_properties(LVUI.NativeMeasuredCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    endif()
    if(TARGET LVUINativeVirtualCollectionTests)
    foreach(_case vertical grid anchor lifetime diagnostics indexed reveal)
        add_test(NAME LVUI.NativeVirtualCollection.${_case} COMMAND LVUINativeVirtualCollectionTests ${_case})
        set_tests_properties(LVUI.NativeVirtualCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    endif()
    foreach(_case identity rollback range lifetime diagnostics sparse)
        add_test(NAME LVUI.NativeCollection.${_case} COMMAND LVUINativeCollectionTests ${_case})
        set_tests_properties(LVUI.NativeCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LUIViewSessionTests tests/view_sessions.cpp)
    target_link_libraries(LUIViewSessionTests PRIVATE LVUI::LVUI)
    foreach(_case IN ITEMS syntax diagnostics capture queries connections localization objects callbacks)
        add_test(NAME LVUI.ViewSessions.${_case} COMMAND LUIViewSessionTests ${_case})
        set_tests_properties(LVUI.ViewSessions.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUISceneCompileTests tests/scene.cpp)
    target_link_libraries(LVUISceneCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.Scene.Codegen COMMAND LVUISceneCompileTests)
    set_tests_properties(LVUI.Scene.Codegen PROPERTIES TIMEOUT 15)
    add_executable(LVUIComponentTests tests/components.cpp)
    add_executable(LVUIViewSequenceTests tests/view_sequences.cpp)
    target_link_libraries(LVUIViewSequenceTests PRIVATE LVUI::LVUI)
    foreach(_case syntax composition typing capture diagnostics limits)
        add_test(NAME LVUI.ViewSequences.${_case} COMMAND LVUIViewSequenceTests ${_case})
        set_tests_properties(LVUI.ViewSequences.${_case} PROPERTIES TIMEOUT 30)
    endforeach()
    target_link_libraries(LVUIComponentTests PRIVATE LVUI::LVUI)
    foreach(_case syntax borrowing slots defaults diagnostics limits callbacks callback_diagnostics callback_limits function_values function_variables)
        add_test(NAME LVUI.Components.${_case} COMMAND LVUIComponentTests ${_case})
        set_tests_properties(LVUI.Components.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIConditionalTests tests/conditional_views.cpp)
    target_link_libraries(LVUIConditionalTests PRIVATE LVUI::LVUI)
    foreach(_case syntax native diagnostics)
        add_test(NAME LVUI.Conditional.${_case} COMMAND LVUIConditionalTests ${_case})
        set_tests_properties(LVUI.Conditional.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIViewValueTests tests/view_values.cpp)
    target_link_libraries(LVUIViewValueTests PRIVATE LVUI::LVUI)
    foreach(_case syntax captured identity root inputs diagnostics)
        add_test(NAME LVUI.ViewValues.${_case} COMMAND LVUIViewValueTests ${_case})
        set_tests_properties(LVUI.ViewValues.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    set(LVUI_IIGUI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iiGUI" CACHE PATH "Optional iiGUI public headers for generated C++ compilation")
    find_path(LVUI_IIGUI_HEADER_DIR NAMES iiGUI/Export.h
        HINTS "$ENV{HOME}/.local/SDK/iiGUI/include" "$ENV{USERPROFILE}/.local/SDK/iiGUI/include" NO_DEFAULT_PATH)
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/Widgets/NativeWidgets.h" AND
       EXISTS "${LVUI_IIGUI_HEADER_DIR}/iiGUI/Export.h")
        # Optional integration fixtures compile against the checked-out iiGUI
        # source while taking its generated export macro from the package.
        # Do not rely on the other project's disposable build directory.
        set(LVUI_IIGUI_TEST_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/iiGUI-test-headers")
        foreach(_area IN ITEMS API Core Graphics Platform Presentation Text Widgets)
            file(GLOB _headers CONFIGURE_DEPENDS "${LVUI_IIGUI_SOURCE_DIR}/src/${_area}/*.h" "${LVUI_IIGUI_SOURCE_DIR}/src/${_area}/*.hpp")
            foreach(_header IN LISTS _headers)
                get_filename_component(_name "${_header}" NAME)
                if(DEFINED _lvui_iigui_header_${_name})
                    message(FATAL_ERROR "Duplicate iiGUI test header basename: ${_name}")
                endif()
                set("_lvui_iigui_header_${_name}" "${_header}")
                file(GENERATE OUTPUT "${LVUI_IIGUI_TEST_HEADERS}/iiGUI/${_name}"
                    CONTENT "#pragma once\n#include \"${_header}\"\n")
            endforeach()
        endforeach()
        add_test(NAME LVUI.IiGuiHeaderOverlay COMMAND ${CMAKE_COMMAND}
            "-DOVERLAY=${LVUI_IIGUI_TEST_HEADERS}" "-DGUI_SOURCE=${LVUI_IIGUI_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/ii_gui_overlay.cmake")
        if(TARGET LVUINativeApplicationFixture)
            add_executable(LVUINativeApplicationTests tests/native_application.cpp "${_native_application_generated}")
            target_include_directories(LVUINativeApplicationTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeApplicationTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeApplication.Headless COMMAND LVUINativeApplicationTests)
            set_tests_properties(LVUI.NativeApplication.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeForegroundFixture AND TARGET LVUINativeForegroundMarkupFixture)
            add_executable(LVUINativeForegroundTests tests/native_application_foreground.cpp
                "${_native_foreground_generated}" "${_native_foreground_markup_generated}")
            target_include_directories(LVUINativeForegroundTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeForegroundTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeForeground.Headless COMMAND LVUINativeForegroundTests)
            set_tests_properties(LVUI.NativeForeground.Headless PROPERTIES TIMEOUT 60)
        endif()
        if(TARGET LVUINativeLaunchFixture)
            add_executable(LVUINativeLaunchTests tests/native_launch.cpp "${_native_launch_generated}")
            target_include_directories(LVUINativeLaunchTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeLaunchTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeLaunch.Headless COMMAND LVUINativeLaunchTests)
            set_tests_properties(LVUI.NativeLaunch.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeCloseFixture)
            add_executable(LVUINativeCloseTests tests/native_close.cpp "${_native_close_generated}")
            target_include_directories(LVUINativeCloseTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeCloseTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeClose.Headless COMMAND LVUINativeCloseTests)
            set_tests_properties(LVUI.NativeClose.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeCloseScenesFixture)
            add_executable(LVUINativeCloseScenesTests tests/native_close_scenes.cpp "${_native_close_scenes_generated}")
            target_include_directories(LVUINativeCloseScenesTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures)
            target_link_libraries(LVUINativeCloseScenesTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeCloseScenes.Headless COMMAND LVUINativeCloseScenesTests)
            set_tests_properties(LVUI.NativeCloseScenes.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeWindowCloseFixture AND TARGET LVUINativeSingleWindowCloseFixture)
            add_executable(LVUINativeWindowCloseTests tests/native_window_close.cpp
                "${_native_window_close_generated}" "${_native_single_window_close_generated}")
            target_include_directories(LVUINativeWindowCloseTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeWindowCloseTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeWindowClose.Headless COMMAND LVUINativeWindowCloseTests)
            set_tests_properties(LVUI.NativeWindowClose.Headless PROPERTIES TIMEOUT 60)
        endif()
        if(TARGET LVUINativeQuitOnlyFixture)
            add_executable(LVUINativeQuitStartupTests tests/native_quit_startup.cpp "${_native_quit_generated}")
            target_include_directories(LVUINativeQuitStartupTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
            target_link_libraries(LVUINativeQuitStartupTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeQuitStartup.Headless COMMAND LVUINativeQuitStartupTests)
            set_tests_properties(LVUI.NativeQuitStartup.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeMultiWindowFixture)
            add_executable(LVUINativeMultiWindowTests tests/native_multi_window.cpp "${_native_multi_window_generated}")
            target_include_directories(LVUINativeMultiWindowTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeMultiWindowTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeMultiWindow.Headless COMMAND LVUINativeMultiWindowTests)
            set_tests_properties(LVUI.NativeMultiWindow.Headless PROPERTIES TIMEOUT 30)
            if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND EXISTS "${LVUI_IIGUI_SOURCE_DIR}/tests/wayland_mocks/WaylandConnectionMock.cpp")
                set(_wayland_caps "${LVUI_IIGUI_SOURCE_DIR}/src/Platform/HostCapabilities.cpp")
                set_source_files_properties("${_wayland_caps}" PROPERTIES COMPILE_DEFINITIONS "IIGUI_WAYLAND;IIGUI_STATIC_DEFINE")
                add_executable(LVUIWaylandMockApplicationTests tests/wayland_mock_application.cpp
                    "${_native_multi_window_generated}" "${_native_window_close_generated}" "${_native_foreground_generated}" "${LVUI_IIGUI_SOURCE_DIR}/src/Platform/HostWayland.cpp" "${_wayland_caps}"
                    "${LVUI_IIGUI_SOURCE_DIR}/tests/wayland_mocks/WaylandConnectionMock.cpp"
                    "${LVUI_IIGUI_SOURCE_DIR}/tests/wayland_mocks/WidgetSurfaceMock.cpp")
                target_include_directories(LVUIWaylandMockApplicationTests BEFORE PRIVATE
                    "${LVUI_IIGUI_SOURCE_DIR}/tests/wayland_mocks" "${CMAKE_CURRENT_BINARY_DIR}"
                    "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
                target_link_libraries(LVUIWaylandMockApplicationTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
                add_test(NAME LVUI.WaylandMockApplication.Headless COMMAND LVUIWaylandMockApplicationTests)
                set_tests_properties(LVUI.WaylandMockApplication.Headless PROPERTIES TIMEOUT 60)
            endif()
        endif()
        if(TARGET LVUINativeNamedWindowsFixture)
            add_executable(LVUINativeNamedWindowsTests tests/native_named_windows.cpp "${_native_named_windows_generated}")
            target_include_directories(LVUINativeNamedWindowsTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeNamedWindowsTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeNamedWindows.Headless COMMAND LVUINativeNamedWindowsTests)
            set_tests_properties(LVUI.NativeNamedWindows.Headless PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeScenesFixture)
            add_executable(LVUINativeScenesTests tests/native_scene_generated.cpp "${_native_scenes_generated}")
            target_include_directories(LVUINativeScenesTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeScenesTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.Scene.Native COMMAND LVUINativeScenesTests)
            set_tests_properties(LVUI.Scene.Native PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeSingleSceneFixture)
            add_executable(LVUINativeSingleSceneTests tests/native_scene_single.cpp "${_native_single_scene_generated}")
            target_include_directories(LVUINativeSingleSceneTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeSingleSceneTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.Scene.SingleNative COMMAND LVUINativeSingleSceneTests)
            set_tests_properties(LVUI.Scene.SingleNative PROPERTIES TIMEOUT 30)
        endif()
        if(TARGET LVUINativeDynamicWindowsFixture AND TARGET LVUINativeDynamicWindowsMarkupFixture)
            add_executable(LVUINativeDynamicWindowsTests tests/native_dynamic_windows.cpp
                "${_native_dynamic_windows_generated}" "${_native_dynamic_windows_markup_generated}")
            target_include_directories(LVUINativeDynamicWindowsTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(LVUINativeDynamicWindowsTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeDynamicWindows.Headless COMMAND LVUINativeDynamicWindowsTests)
            set_tests_properties(LVUI.NativeDynamicWindows.Headless PROPERTIES TIMEOUT 30)
        endif()
        lvui_generate(NAME NativeOwnedProgress SOURCE tests/fixtures/NativeOwnedProgress.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _owned_progress_generated)
        lvui_generate(NAME NativeOwnedProgressAdvanced SOURCE tests/fixtures/NativeOwnedProgressAdvanced.lvui
            INCLUDES NativeOwnedProgressFixture.h OUTPUT_VARIABLE _owned_progress_advanced)
        lvui_generate(NAME NativeOwnedProgressSource SOURCE tests/fixtures/NativeOwnedProgressSource.lvui
            INCLUDES NativeOwnedProgressFixture.h OUTPUT_VARIABLE _owned_progress_source)
        lvui_generate(NAME NativeOwnedProgressScoped SOURCE tests/fixtures/NativeOwnedProgressScoped.lvui
            OUTPUT_VARIABLE _owned_progress_scoped)
        lvui_generate(NAME NativeOwnedProgressMotion SOURCE tests/fixtures/NativeOwnedProgressMotion.lvui
            INCLUDES NativeOwnedProgressFixture.h OUTPUT_VARIABLE _owned_progress_motion)
        add_library(LVUIOwnedProgressCompile OBJECT tests/native_owned_progress_compile.cpp "${_owned_progress_generated}" "${_owned_progress_advanced}" "${_owned_progress_source}" "${_owned_progress_scoped}" "${_owned_progress_motion}")
        target_include_directories(LVUIOwnedProgressCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedProgressCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedImage SOURCE tests/fixtures/NativeOwnedImage.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_generated)
        lvui_generate(NAME NativeOwnedImageAdvanced SOURCE tests/fixtures/NativeOwnedImageAdvanced.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_advanced)
        lvui_generate(NAME NativeOwnedImageStatic SOURCE tests/fixtures/NativeOwnedImageStatic.lvui
            OUTPUT_VARIABLE _owned_image_static)
        add_library(LVUIOwnedImageCompile OBJECT tests/native_owned_image_compile.cpp "${_owned_image_generated}" "${_owned_image_advanced}" "${_owned_image_static}")
        target_include_directories(LVUIOwnedImageCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedImageCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedButton SOURCE tests/fixtures/NativeOwnedButton.lvui
            INCLUDES NativeOwnedButtonFixture.h OUTPUT_VARIABLE _owned_button_generated)
        add_library(LVUIOwnedButtonCompile OBJECT tests/native_owned_button_compile.cpp "${_owned_button_generated}")
        target_include_directories(LVUIOwnedButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME FigmaComponentDeclarations SOURCE tests/fixtures/FigmaComponentDeclarations.lvui
            OUTPUT_VARIABLE _figma_components_generated)
        add_library(LVUIFigmaComponentCompile OBJECT tests/figma_component_compile.cpp "${_figma_components_generated}")
        target_include_directories(LVUIFigmaComponentCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIFigmaComponentCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedDivider SOURCE tests/fixtures/NativeOwnedDivider.lvui
            INCLUDES NativeOwnedDividerFixture.h OUTPUT_VARIABLE _owned_divider_generated)
        add_library(LVUIOwnedDividerCompile OBJECT tests/native_owned_divider_compile.cpp "${_owned_divider_generated}")
        target_include_directories(LVUIOwnedDividerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedDividerCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedToggleSwitch SOURCE tests/fixtures/NativeOwnedToggleSwitch.lvui
            INCLUDES NativeOwnedToggleSwitchFixture.h OUTPUT_VARIABLE _owned_toggle_generated)
        add_library(LVUIOwnedToggleSwitchCompile OBJECT tests/native_owned_toggle_switch_compile.cpp "${_owned_toggle_generated}")
        target_include_directories(LVUIOwnedToggleSwitchCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedToggleSwitchCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedCheckBox SOURCE tests/fixtures/NativeOwnedCheckBox.lvui
            INCLUDES NativeOwnedCheckBoxFixture.h OUTPUT_VARIABLE _owned_checkbox_generated)
        add_library(LVUIOwnedCheckBoxCompile OBJECT tests/native_owned_check_box_compile.cpp "${_owned_checkbox_generated}")
        target_include_directories(LVUIOwnedCheckBoxCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedCheckBoxCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedRadioButton SOURCE tests/fixtures/NativeOwnedRadioButton.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_generated)
        lvui_generate(NAME NativeOwnedRadioRows SOURCE tests/fixtures/NativeOwnedRadioRows.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_rows_generated)
        add_library(LVUIOwnedRadioButtonCompile OBJECT tests/native_owned_radio_button_compile.cpp "${_owned_radio_generated}" "${_owned_radio_rows_generated}")
        target_include_directories(LVUIOwnedRadioButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedRadioButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedMaterial SOURCE tests/fixtures/NativeOwnedMaterial.lvui
            INCLUDES NativeOwnedMaterialFixture.h OUTPUT_VARIABLE _owned_material_generated)
        add_library(LVUIOwnedMaterialCompile OBJECT tests/native_owned_material_compile.cpp "${_owned_material_generated}")
        target_include_directories(LVUIOwnedMaterialCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedMaterialCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedLink SOURCE tests/fixtures/NativeOwnedLink.lvui
            INCLUDES NativeOwnedLinkFixture.h OUTPUT_VARIABLE _owned_link_generated)
        lvui_generate(NAME NativeOwnedLinkMethods SOURCE tests/fixtures/NativeOwnedLinkMethods.lvui
            INCLUDES NativeOwnedLinkMethodsFixture.h OUTPUT_VARIABLE _owned_link_methods_generated)
        add_library(LVUIOwnedLinkCompile OBJECT tests/native_owned_link_compile.cpp "${_owned_link_generated}" "${_owned_link_methods_generated}")
        target_include_directories(LVUIOwnedLinkCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedLinkCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedPageRouter SOURCE tests/fixtures/NativeOwnedPageRouter.lvui
            INCLUDES NativeOwnedPageRouterFixture.h OUTPUT_VARIABLE _owned_page_router_generated)
        add_library(LVUIOwnedPageRouterCompile OBJECT tests/native_owned_page_router_compile.cpp "${_owned_page_router_generated}")
        target_include_directories(LVUIOwnedPageRouterCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedPageRouterCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppCard SOURCE tests/fixtures/NativeOwnedAppCard.lvui
            INCLUDES NativeOwnedAppCardFixture.h OUTPUT_VARIABLE _owned_app_card_generated)
        add_library(LVUIOwnedAppCardCompile OBJECT tests/native_owned_app_card_compile.cpp "${_owned_app_card_generated}")
        target_include_directories(LVUIOwnedAppCardCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedAppCardCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppHeader SOURCE tests/fixtures/NativeOwnedAppHeader.lvui
            INCLUDES NativeOwnedAppHeaderFixture.h OUTPUT_VARIABLE _owned_app_header_generated)
        add_library(LVUIOwnedAppHeaderCompile OBJECT tests/native_owned_app_header_compile.cpp "${_owned_app_header_generated}")
        target_include_directories(LVUIOwnedAppHeaderCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedAppHeaderCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedEventListener SOURCE tests/fixtures/NativeOwnedEventListener.lvui
            INCLUDES NativeOwnedEventListenerFixture.h OUTPUT_VARIABLE _owned_event_listener_generated)
        add_library(LVUIOwnedEventListenerCompile OBJECT tests/native_owned_event_listener_compile.cpp "${_owned_event_listener_generated}")
        target_include_directories(LVUIOwnedEventListenerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedEventListenerCompile PRIVATE LVUI::Native)
        option(LVUI_TEST_NATIVE_EVENT_HOST "Run event-listener fixtures with the matching iiGUI host" OFF)
        if(LVUI_TEST_NATIVE_EVENT_HOST)
            find_package(iiGUI 0.5.0 CONFIG REQUIRED)
            lvui_generate(NAME ImmediateDeclaration SOURCE tests/fixtures/ImmediateDeclaration.lvui
                INCLUDES ImmediateDeclarationBackend.h OUTPUT_VARIABLE _immediate_declaration_generated)
            add_executable(LVUIImmediateDeclarationTests tests/immediate_declaration_generated.cpp "${_immediate_declaration_generated}")
            target_include_directories(LVUIImmediateDeclarationTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUIImmediateDeclarationTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.ImmediateDeclaration.Generated COMMAND LVUIImmediateDeclarationTests)
            lvui_generate(NAME NativeOwnedScroll SOURCE tests/fixtures/NativeOwnedScroll.lvui
                INCLUDES NativeOwnedScrollFixture.h OUTPUT_VARIABLE _owned_scroll_generated)
            add_executable(LVUIHostScrollTests tests/native_owned_scroll.cpp "${_owned_scroll_generated}")
            target_include_directories(LVUIHostScrollTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUIHostScrollTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.NativeScroll.Host COMMAND LVUIHostScrollTests)
            set_tests_properties(LVUI.NativeScroll.Host PROPERTIES TIMEOUT 60)
            add_executable(LVUIHostEventFlowTests tests/native_owned_event_listener.cpp "${_owned_event_listener_generated}")
            target_include_directories(LVUIHostEventFlowTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUIHostEventFlowTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.NativeEventFlow.Host COMMAND LVUIHostEventFlowTests)
            set_tests_properties(LVUI.NativeEventFlow.Host PROPERTIES TIMEOUT 60)
            if(APPLE)
                add_executable(LVUIHostInputProbe MACOSX_BUNDLE tests/native_input_probe.cpp)
                set_target_properties(LVUIHostInputProbe PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.lvui.input-probe")
            else()
                add_executable(LVUIHostInputProbe tests/native_input_probe.cpp)
            endif()
            target_link_libraries(LVUIHostInputProbe PRIVATE LVUI::Native iiGUI::iiGUI)
        endif()
        lvui_generate(NAME NativeOwnedAvatar SOURCE tests/fixtures/NativeOwnedAvatar.lvui
            INCLUDES NativeOwnedAvatarFixture.h OUTPUT_VARIABLE _owned_avatar_generated)
        add_library(LVUIOwnedAvatarCompile OBJECT tests/native_owned_avatar_compile.cpp "${_owned_avatar_generated}")
        target_include_directories(LVUIOwnedAvatarCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUIOwnedAvatarCompile PRIVATE LVUI::Native)
        if(TARGET iiGUI::iiGUI AND TARGET iiSDKCore::Runtime)
            add_executable(LVUITooltipModelIntegrationTests tests/tooltip_model_integration.cpp)
            target_link_libraries(LVUITooltipModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Tooltip.ModelIntegration COMMAND LVUITooltipModelIntegrationTests)
            add_executable(LVUITooltipControllerIntegrationTests tests/tooltip_controller_integration.cpp)
            target_link_libraries(LVUITooltipControllerIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Tooltip.ControllerIntegration COMMAND LVUITooltipControllerIntegrationTests)
            add_executable(LVUIHierarchyPanelLayoutIntegrationTests tests/panel_hierarchy_layout_integration.cpp)
            target_link_libraries(LVUIHierarchyPanelLayoutIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.PanelLayout.HierarchyIntegration COMMAND LVUIHierarchyPanelLayoutIntegrationTests)
            add_executable(LVUIListPanelLayoutIntegrationTests tests/panel_list_layout_integration.cpp)
            target_link_libraries(LVUIListPanelLayoutIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.PanelLayout.ListIntegration COMMAND LVUIListPanelLayoutIntegrationTests)
            add_executable(LVUITableHeaderModelIntegrationTests tests/table_header_model_integration.cpp)
            target_link_libraries(LVUITableHeaderModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.TableHeader.ModelIntegration COMMAND LVUITableHeaderModelIntegrationTests)
            add_executable(LVUIHierarchyToolbarModelIntegrationTests tests/hierarchy_toolbar_model_integration.cpp)
            target_link_libraries(LVUIHierarchyToolbarModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.HierarchyToolbar.ModelIntegration COMMAND LVUIHierarchyToolbarModelIntegrationTests)
            add_executable(LVUIHierarchyItemModelIntegrationTests tests/hierarchy_item_model_integration.cpp)
            target_link_libraries(LVUIHierarchyItemModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.HierarchyItem.ModelIntegration COMMAND LVUIHierarchyItemModelIntegrationTests)
            add_executable(LVUIMobileNavigationModelIntegrationTests tests/mobile_navigation_model_integration.cpp)
            target_link_libraries(LVUIMobileNavigationModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.MobileNavigation.ModelIntegration COMMAND LVUIMobileNavigationModelIntegrationTests)
            add_executable(LVUIMobileNavigationMotionIntegrationTests tests/mobile_navigation_motion_integration.cpp)
            target_link_libraries(LVUIMobileNavigationMotionIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.MobileNavigation.MotionIntegration COMMAND LVUIMobileNavigationMotionIntegrationTests)
            add_executable(LVUIColorPickerLayoutIntegrationTests tests/color_picker_layout_integration.cpp)
            target_link_libraries(LVUIColorPickerLayoutIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ColorPicker.LayoutIntegration COMMAND LVUIColorPickerLayoutIntegrationTests)
            add_executable(LVUITableCellModelIntegrationTests tests/table_cell_model_integration.cpp)
            target_link_libraries(LVUITableCellModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.TableCell.ModelIntegration COMMAND LVUITableCellModelIntegrationTests)
            add_executable(LVUITableRowModelIntegrationTests tests/table_row_model_integration.cpp)
            target_link_libraries(LVUITableRowModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.TableRow.ModelIntegration COMMAND LVUITableRowModelIntegrationTests)
            add_executable(LVUISliderModelIntegrationTests tests/slider_model_integration.cpp)
            target_link_libraries(LVUISliderModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Slider.ModelIntegration COMMAND LVUISliderModelIntegrationTests)
            add_executable(LVUISliderFilledIntegrationTests tests/native_filled_slider.cpp)
            target_link_libraries(LVUISliderFilledIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Slider.FilledIntegration COMMAND LVUISliderFilledIntegrationTests)
            add_executable(LVUITextInputModelIntegrationTests tests/native_text_input_model.cpp)
            target_link_libraries(LVUITextInputModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.TextInput.ModelIntegration COMMAND LVUITextInputModelIntegrationTests)
            if(TARGET LVUIExampleColorAdapter)
                add_executable(LVUIColorChannelSurfaceIntegrationTests tests/native_color_channel_surface_integration.cpp)
                target_link_libraries(LVUIColorChannelSurfaceIntegrationTests PRIVATE LVUIExampleColorAdapter iiGUI::iiGUI)
                add_test(NAME LVUI.ColorChannelSurface.ModelIntegration COMMAND LVUIColorChannelSurfaceIntegrationTests)
            endif()
            add_executable(LVUIListItemSelectorModelIntegrationTests tests/native_list_item_selector_model.cpp)
            target_link_libraries(LVUIListItemSelectorModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ListItemSelector.ModelIntegration COMMAND LVUIListItemSelectorModelIntegrationTests)
            add_executable(LVUIListItemSegmentsModelIntegrationTests tests/native_list_item_segments_model.cpp)
            target_link_libraries(LVUIListItemSegmentsModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ListItemSegments.ModelIntegration COMMAND LVUIListItemSegmentsModelIntegrationTests)
            add_executable(LVUICardModelIntegrationTests tests/native_card_model.cpp)
            target_link_libraries(LVUICardModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Card.ModelIntegration COMMAND LVUICardModelIntegrationTests)
            add_executable(LVUITabModelIntegrationTests tests/native_tab_model.cpp)
            target_link_libraries(LVUITabModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Tab.ModelIntegration COMMAND LVUITabModelIntegrationTests)
            add_executable(LVUIColorSurfaceModelIntegrationTests tests/native_color_surface_model.cpp)
            target_link_libraries(LVUIColorSurfaceModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ColorSurface.ModelIntegration COMMAND LVUIColorSurfaceModelIntegrationTests)
            add_executable(LVUILinkModelIntegrationTests tests/native_link_model.cpp)
            target_link_libraries(LVUILinkModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Link.ModelIntegration COMMAND LVUILinkModelIntegrationTests)
            add_executable(LVUIComboBoxModelIntegrationTests tests/native_combo_box_model.cpp)
            target_link_libraries(LVUIComboBoxModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ComboBox.ModelIntegration COMMAND LVUIComboBoxModelIntegrationTests)
            add_executable(LVUIColorButtonModelIntegrationTests tests/native_color_button_model.cpp)
            target_link_libraries(LVUIColorButtonModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.ColorButton.ModelIntegration COMMAND LVUIColorButtonModelIntegrationTests)
            add_executable(LVUIPopoverModelIntegrationTests tests/native_popover_model.cpp)
            target_link_libraries(LVUIPopoverModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Popover.ModelIntegration COMMAND LVUIPopoverModelIntegrationTests)
            add_executable(LVUIMaterialModelIntegrationTests tests/native_material_model.cpp)
            target_link_libraries(LVUIMaterialModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Material.ModelIntegration COMMAND LVUIMaterialModelIntegrationTests)
            add_executable(LVUIMenuModelIntegrationTests tests/native_menu_model.cpp)
            target_link_libraries(LVUIMenuModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.Menu.ModelIntegration COMMAND LVUIMenuModelIntegrationTests)
            add_executable(LVUIMenuItemModelIntegrationTests tests/native_menu_item_model.cpp)
            target_link_libraries(LVUIMenuItemModelIntegrationTests PRIVATE LVUI::Native iiGUI::iiGUI)
            add_test(NAME LVUI.MenuItem.ModelIntegration COMMAND LVUIMenuItemModelIntegrationTests)
            lvui_generate(NAME NativeDefaultBinding SOURCE tests/fixtures/NativeDefaultBinding.lvui
                INCLUDES NativeDefaultBindingFixture.h OUTPUT_VARIABLE _native_default_binding_generated)
            add_executable(LVUINativeDefaultBindingTests tests/native_default_binding.cpp "${_native_default_binding_generated}")
            target_include_directories(LVUINativeDefaultBindingTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeDefaultBindingTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeDefaultBinding COMMAND LVUINativeDefaultBindingTests)
            lvui_generate(NAME NativeQueryValue SOURCE tests/fixtures/NativeQueryValue.lvui
                INCLUDES NativeQueryValueFixture.h OUTPUT_VARIABLE _native_query_value_generated)
            add_executable(LVUINativeQueryValueTests tests/native_query_value.cpp "${_native_query_value_generated}")
            target_include_directories(LVUINativeQueryValueTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeQueryValueTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeQueryValue COMMAND LVUINativeQueryValueTests)
            set_tests_properties(LVUI.NativeQueryValue PROPERTIES TIMEOUT 60)
            lvui_generate(NAME NativeViewOnly SOURCE example/GettingStarted/CppViewModelCounter.lvui
                INCLUDES CppViewModelCounter.h OUTPUT_VARIABLE _native_view_only_generated)
            add_executable(LVUINativeViewOnlyTests tests/native_view_only.cpp "${_native_view_only_generated}")
            target_include_directories(LVUINativeViewOnlyTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                example/GettingStarted "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeViewOnlyTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeViewOnly COMMAND LVUINativeViewOnlyTests)
            lvui_generate(NAME ViewSessionCounter SOURCE example/GettingStarted/ViewSessionCounter.lvui
                OUTPUT_VARIABLE _view_session_counter)
            lvui_generate(NAME ViewSessionIntegration SOURCE tests/fixtures/ViewSessionIntegration.lvui
                INCLUDES ViewSessionIntegrationFixture.h OUTPUT_VARIABLE _view_session_integration)
            add_executable(LVUINativeViewSessionTests tests/native_view_sessions.cpp
                "${_view_session_counter}" "${_view_session_integration}")
            target_include_directories(LVUINativeViewSessionTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeViewSessionTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.ViewSessions.Native COMMAND LVUINativeViewSessionTests)
            add_executable(LVUINativeQueryConnectionsTests tests/native_query_connections.cpp)
            target_include_directories(LVUINativeQueryConnectionsTests PRIVATE "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeQueryConnectionsTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.QueryConnections.Native COMMAND LVUINativeQueryConnectionsTests)
            lvui_generate(NAME NativeTextEditor SOURCE tests/fixtures/NativeTextEditor.lvui
                INCLUDES NativeTextEditorFixture.h OUTPUT_VARIABLE _native_text_editor_generated)
            add_executable(LVUINativeTextEditorGeneratedTests tests/native_text_editor_generated.cpp "${_native_text_editor_generated}")
            target_include_directories(LVUINativeTextEditorGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeTextEditorGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.TextEditor.Generated COMMAND LVUINativeTextEditorGeneratedTests)
            lvui_generate(NAME NativeMobileNavigation SOURCE tests/fixtures/NativeMobileNavigation.lvui
                INCLUDES NativeMobileNavigationFixture.h OUTPUT_VARIABLE _native_mobile_navigation_generated)
            add_executable(LVUINativeMobileNavigationGeneratedTests tests/native_mobile_navigation_generated.cpp "${_native_mobile_navigation_generated}")
            target_include_directories(LVUINativeMobileNavigationGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileNavigationGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileNavigation.Generated COMMAND LVUINativeMobileNavigationGeneratedTests)
            add_executable(LVUINativeMobileNavigationEffectsTests tests/native_mobile_navigation_effects.cpp)
            target_include_directories(LVUINativeMobileNavigationEffectsTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileNavigationEffectsTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileNavigation.NativeEffects COMMAND LVUINativeMobileNavigationEffectsTests)
            add_executable(LVUINativeMobileTabTests tests/native_mobile_tab.cpp)
            target_include_directories(LVUINativeMobileTabTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileTabTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileTab.Native COMMAND LVUINativeMobileTabTests)
            add_executable(LVUINativeMobileTabMotionTests tests/native_mobile_tab_motion.cpp)
            target_include_directories(LVUINativeMobileTabMotionTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileTabMotionTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileTab.Motion COMMAND LVUINativeMobileTabMotionTests)
            add_executable(LVUINativeMobileTabReentrantTests tests/native_mobile_tab_reentrant.cpp)
            target_include_directories(LVUINativeMobileTabReentrantTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileTabReentrantTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileTab.Reentrant COMMAND LVUINativeMobileTabReentrantTests)
            lvui_generate(NAME NativeMobileTabs SOURCE tests/fixtures/NativeMobileTabs.lvui
                INCLUDES NativeMobileTabsFixture.h OUTPUT_VARIABLE _native_mobile_tabs_generated)
            add_executable(LVUINativeMobileTabsGeneratedTests tests/native_mobile_tabs_generated.cpp "${_native_mobile_tabs_generated}")
            target_include_directories(LVUINativeMobileTabsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileTabsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileTab.Generated COMMAND LVUINativeMobileTabsGeneratedTests)
            add_executable(LVUINativeEditorCanvasTests tests/native_editor_canvas.cpp)
            target_include_directories(LVUINativeEditorCanvasTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeEditorCanvasTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeEditorCanvas COMMAND LVUINativeEditorCanvasTests)
            add_executable(LVUINativeSheetRetentionTests tests/native_owned_sheet.cpp)
            target_include_directories(LVUINativeSheetRetentionTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeSheetRetentionTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeSheet.Retention COMMAND LVUINativeSheetRetentionTests)
            add_executable(LVUINativeCardOpenTests tests/native_card_open.cpp)
            target_include_directories(LVUINativeCardOpenTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeCardOpenTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.CardOpen.Native COMMAND LVUINativeCardOpenTests)
            lvui_generate(NAME NativeCardOpen SOURCE tests/fixtures/NativeCardOpen.lvui
                INCLUDES NativeCardOpenFixture.h OUTPUT_VARIABLE _native_card_open_generated)
            add_executable(LVUINativeCardOpenGeneratedTests tests/native_card_open_generated.cpp "${_native_card_open_generated}")
            target_include_directories(LVUINativeCardOpenGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeCardOpenGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.CardOpen.Generated COMMAND LVUINativeCardOpenGeneratedTests)
            add_executable(LVUINativeEditorTimelineTests tests/native_editor_timeline.cpp)
            target_include_directories(LVUINativeEditorTimelineTests BEFORE PRIVATE
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeEditorTimelineTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeEditorTimeline COMMAND LVUINativeEditorTimelineTests)
            lvui_generate(NAME NativeEditorViews SOURCE tests/fixtures/NativeEditorViews.lvui
                INCLUDES NativeEditorViewsFixture.h OUTPUT_VARIABLE _native_editor_views_generated)
            add_executable(LVUINativeEditorViewsGeneratedTests tests/native_editor_views_generated.cpp "${_native_editor_views_generated}")
            target_include_directories(LVUINativeEditorViewsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeEditorViewsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeEditorViews.Generated COMMAND LVUINativeEditorViewsGeneratedTests)
            lvui_generate(NAME NativeMobileTabBar SOURCE tests/fixtures/NativeMobileTabBar.lvui
                INCLUDES NativeMobileNavigationFixture.h OUTPUT_VARIABLE _native_mobile_tab_bar_generated)
            add_executable(LVUINativeMobileTabBarGeneratedTests tests/native_mobile_tab_bar_generated.cpp "${_native_mobile_tab_bar_generated}")
            target_include_directories(LVUINativeMobileTabBarGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeMobileTabBarGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.MobileTabBar.Generated COMMAND LVUINativeMobileTabBarGeneratedTests)
            lvui_generate(NAME NativeAccountButton SOURCE tests/fixtures/NativeAccountButton.lvui
                INCLUDES NativeOwnedInputFieldFixture.h OUTPUT_VARIABLE _native_account_button_generated)
            add_executable(LVUINativeAccountButtonTests tests/native_account_button.cpp "${_native_account_button_generated}")
            target_include_directories(LVUINativeAccountButtonTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeAccountButtonTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.AccountButton.Generated COMMAND LVUINativeAccountButtonTests)
            lvui_generate(NAME NativeLabelPresentation SOURCE tests/fixtures/NativeLabelPresentation.lvui
                INCLUDES NativeLabelPresentationFixture.h OUTPUT_VARIABLE _native_label_presentation_generated)
            add_executable(LVUINativeLabelPresentationTests tests/native_label_presentation.cpp "${_native_label_presentation_generated}")
            target_include_directories(LVUINativeLabelPresentationTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeLabelPresentationTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.LabelPresentation.Generated COMMAND LVUINativeLabelPresentationTests)
            lvui_generate(NAME NativeListItemValue SOURCE tests/fixtures/NativeListItemValue.lvui
                INCLUDES NativeListItemValueFixture.h OUTPUT_VARIABLE _native_list_item_value_generated)
            add_executable(LVUINativeListItemValueTests tests/native_list_item_value.cpp "${_native_list_item_value_generated}")
            target_include_directories(LVUINativeListItemValueTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeListItemValueTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.ListItemValue.Generated COMMAND LVUINativeListItemValueTests)
            lvui_generate(NAME NativeListItemDetail SOURCE tests/fixtures/NativeListItemDetail.lvui
                INCLUDES NativeListItemDetailFixture.h OUTPUT_VARIABLE _native_list_item_detail_generated)
            add_executable(LVUINativeListItemDetailTests tests/native_list_item_detail.cpp "${_native_list_item_detail_generated}")
            target_include_directories(LVUINativeListItemDetailTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeListItemDetailTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.ListItemDetail.Generated COMMAND LVUINativeListItemDetailTests)
            if(EXISTS "${LVUI_IIGUI_HEADER_DIR}/iiGUI/NativeWidgets.h")
                file(STRINGS "${LVUI_IIGUI_HEADER_DIR}/iiGUI/NativeWidgets.h" _lvui_reveal_api REGEX "^#define IIGUI_REVEAL_LOCAL_RECT_API 1$")
                if(_lvui_reveal_api)
                    lvui_generate(NAME NativeTextEditorScroll SOURCE tests/fixtures/NativeTextEditorScroll.lvui
                        INCLUDES NativeTextEditorFixture.h OUTPUT_VARIABLE _native_text_editor_scroll_generated)
                    add_executable(LVUINativeTextEditorScrollTests tests/native_text_editor_scroll.cpp "${_native_text_editor_scroll_generated}")
                    target_include_directories(LVUINativeTextEditorScrollTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                        tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
                    target_link_libraries(LVUINativeTextEditorScrollTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
                    add_test(NAME LVUI.TextEditor.Scroll COMMAND LVUINativeTextEditorScrollTests)
                endif()
            endif()
            lvui_generate(NAME NativeQueryConnections SOURCE tests/fixtures/NativeQueryConnections.lvui
                INCLUDES NativeQueryConnectionsFixture.h OUTPUT_VARIABLE _query_connections_generated)
            lvui_generate(NAME NativeQueryScene SOURCE tests/fixtures/NativeQueryScene.lvui
                INCLUDES NativeQueryConnectionsFixture.h OUTPUT_VARIABLE _query_scene_generated)
            add_executable(LVUINativeQueryConnectionsGeneratedTests tests/native_query_connections_generated.cpp
                "${_query_connections_generated}" "${_query_scene_generated}")
            target_include_directories(LVUINativeQueryConnectionsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeQueryConnectionsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.QueryConnections.Generated COMMAND LVUINativeQueryConnectionsGeneratedTests)
            lvui_generate(NAME NativeQueryPeer SOURCE tests/fixtures/NativeQueryPeer.lvui
                INCLUDES NativeQueryPeerFixture.h OUTPUT_VARIABLE _query_peer_generated)
            add_executable(LVUINativeQueryPeerTests tests/native_query_peer_generated.cpp "${_query_peer_generated}")
            target_include_directories(LVUINativeQueryPeerTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeQueryPeerTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.QueryPeer.Generated COMMAND LVUINativeQueryPeerTests)
            lvui_generate(NAME LocalizedCounter SOURCE example/GettingStarted/LocalizedCounter.lvui
                OUTPUT_VARIABLE _localized_counter)
            lvui_generate(NAME NativeLocalizedScenes SOURCE tests/fixtures/NativeLocalizedScenes.lvui
                OUTPUT_VARIABLE _localized_scenes)
            lvui_generate(NAME NativeLocalizedApplication SOURCE example/GettingStarted/LocalizedApplication.lvui
                OUTPUT_VARIABLE _localized_application)
            add_executable(LVUINativeAppStringsGeneratedTests tests/native_app_strings_generated.cpp
                "${_localized_counter}" "${_localized_scenes}" "${_localized_application}")
            target_include_directories(LVUINativeAppStringsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeAppStringsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.Localization.Generated COMMAND LVUINativeAppStringsGeneratedTests)
            if(LVUI_BUILD_LOCALIZATION_IO AND LVUI_LOCALIZATION_COMPOSITION_AVAILABLE)
                lvui_add_application(LVUILocalizedApplicationFixture NAME LocalizedApplication
                    SOURCE example/GettingStarted/LocalizedApplication.lvui
                    CONFIGURE_SOURCE example/GettingStarted/App/LocalizedApplication.cpp
                    LIBRARIES LVUI::Localization iiJson::iiJson iiFileProvider::Native)
                target_compile_definitions(LVUILocalizedApplicationFixture PRIVATE
                    "LVUI_EXAMPLE_STRINGS_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/example/GettingStarted/resources/strings\"")
            endif()
            set_tests_properties(LVUI.ViewSessions.Native PROPERTIES TIMEOUT 30)
            lvui_generate(NAME NativeEmbeddedCounter SOURCE tests/fixtures/NativeEmbeddedCounter.lvui
                OUTPUT_VARIABLE _embedded_counter)
            lvui_generate(NAME NativeEmbeddedParent SOURCE tests/fixtures/NativeEmbeddedParent.lvui
                INCLUDES NativeEmbeddedHostBackend.h OUTPUT_VARIABLE _embedded_parent)
            lvui_generate(NAME NativeEmbeddedBranch SOURCE tests/fixtures/NativeEmbeddedBranch.lvui
                INCLUDES NativeEmbeddedBranchBackend.h OUTPUT_VARIABLE _embedded_branch)
            lvui_generate(NAME NativeEmbeddedDual SOURCE tests/fixtures/NativeEmbeddedDual.lvui
                INCLUDES NativeEmbeddedHostBackend.h OUTPUT_VARIABLE _embedded_dual)
            lvui_generate(NAME NativeEmbeddedBadge SOURCE tests/fixtures/NativeEmbeddedBadge.lvui
                OUTPUT_VARIABLE _embedded_badge)
            lvui_generate(NAME NativeEmbeddedRepeated SOURCE tests/fixtures/RepeatedRoot.lvui
                OUTPUT_VARIABLE _embedded_repeated)
            lvui_generate(NAME NativeEmbeddedSequence SOURCE tests/fixtures/NativeEmbeddedSequence.lvui
                OUTPUT_VARIABLE _embedded_sequence)
            lvui_generate(NAME NativeEmbeddedObjectRoot SOURCE tests/fixtures/NativeObjectRoot.lvui
                INCLUDES NativeObjectInputFixture.h OUTPUT_VARIABLE _embedded_object_root)
            lvui_generate(NAME NativeEmbeddedBootstrap SOURCE tests/fixtures/NativeEmbeddedBootstrap.lvui
                OUTPUT_VARIABLE _embedded_bootstrap)
            lvui_generate(NAME NativeEmbeddedShell SOURCE tests/fixtures/NativeEmbeddedShell.lvui
                OUTPUT_VARIABLE _embedded_shell)
            lvui_generate(NAME NativeCompatibilityWindows SOURCE tests/fixtures/NativeCompatibilityWindows.lvui
                OUTPUT_VARIABLE _compatibility_windows)
            add_executable(LVUINativeEmbeddedViewTests tests/native_embedded_generated.cpp
                "${_embedded_counter}" "${_embedded_parent}" "${_embedded_branch}" "${_embedded_dual}" "${_embedded_badge}" "${_embedded_repeated}" "${_embedded_sequence}" "${_embedded_object_root}" "${_embedded_bootstrap}" "${_embedded_shell}" "${_compatibility_windows}")
            target_include_directories(LVUINativeEmbeddedViewTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeEmbeddedViewTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeView.Embedded COMMAND LVUINativeEmbeddedViewTests)
            set_tests_properties(LVUI.NativeView.Embedded PROPERTIES TIMEOUT 30)
            lvui_generate(NAME NativeStandaloneSelector SOURCE tests/fixtures/NativeStandaloneSelector.lvui
                INCLUDES NativeStandaloneSelectorBackend.h OUTPUT_VARIABLE _standalone_selector)
            add_executable(LVUINativeStandaloneSelectorTests tests/native_standalone_selector_generated.cpp "${_standalone_selector}")
            target_include_directories(LVUINativeStandaloneSelectorTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeStandaloneSelectorTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.NativeView.StandaloneSelector COMMAND LVUINativeStandaloneSelectorTests)
            set_tests_properties(LVUI.NativeView.StandaloneSelector PROPERTIES TIMEOUT 30)
            lvui_generate(NAME NativeOwnedCardSlots SOURCE tests/fixtures/NativeOwnedCardSlots.lvui
                INCLUDES NativeOwnedCardSlotsBackend.h OUTPUT_VARIABLE _owned_card_slots)
            add_executable(LVUINativeCardSlotsGeneratedTests tests/native_owned_card_slots_generated.cpp "${_owned_card_slots}")
            target_include_directories(LVUINativeCardSlotsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeCardSlotsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.OwnedCard.SlotsGenerated COMMAND LVUINativeCardSlotsGeneratedTests)
            lvui_generate(NAME NativeOwnedTableViews SOURCE tests/fixtures/NativeOwnedTableViews.lvui
                INCLUDES NativeOwnedTableFixture.h OUTPUT_VARIABLE _owned_table_views)
            lvui_generate(NAME NativeOwnedTableColumnViews SOURCE tests/fixtures/NativeOwnedTableColumnViews.lvui
                INCLUDES NativeOwnedTableFixture.h OUTPUT_VARIABLE _owned_table_column_views)
            lvui_generate(NAME NativeOwnedTableHeaderViews SOURCE tests/fixtures/NativeOwnedTableHeaderViews.lvui
                OUTPUT_VARIABLE _owned_table_header_views)
            lvui_generate(NAME NativeOwnedTableActionViews SOURCE tests/fixtures/NativeOwnedTableActionViews.lvui
                INCLUDES NativeOwnedTableActionBackend.h OUTPUT_VARIABLE _owned_table_action_views)
            add_executable(LVUINativeTableViewsGeneratedTests tests/native_owned_table_views_generated.cpp "${_owned_table_views}" "${_owned_table_column_views}" "${_owned_table_header_views}" "${_owned_table_action_views}")
            target_include_directories(LVUINativeTableViewsGeneratedTests BEFORE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
            target_link_libraries(LVUINativeTableViewsGeneratedTests PRIVATE LVUI::Native iiGUI::iiGUI iiSDKCore::Runtime)
            add_test(NAME LVUI.OwnedTable.ViewsGenerated COMMAND LVUINativeTableViewsGeneratedTests)
        endif()
        lvui_generate(NAME NativeReactive SOURCE tests/fixtures/NativeReactive.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_generated)
        add_executable(LVUINativeGeneratedTests tests/native_generated.cpp "${_native_generated}")
        target_include_directories(LVUINativeGeneratedTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeGeneratedTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeGenerated COMMAND LVUINativeGeneratedTests)
        set_tests_properties(LVUI.NativeGenerated PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeNested SOURCE tests/fixtures/NativeNested.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_nested)
        lvui_generate(NAME NativeRootScope SOURCE tests/fixtures/NativeRootScope.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_root)
        lvui_generate(NAME NativeLocalOnly SOURCE tests/fixtures/NativeLocalOnly.lvui
            OUTPUT_VARIABLE _native_local_only)
        add_executable(LVUINativeNestedTests tests/native_nested.cpp "${_native_nested}" "${_native_root}" "${_native_local_only}")
        target_include_directories(LVUINativeNestedTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeNestedTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeNested COMMAND LVUINativeNestedTests)
        set_tests_properties(LVUI.NativeNested PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeViewValues SOURCE tests/fixtures/NativeViewValues.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_view_values)
        lvui_generate(NAME NativeConditional SOURCE tests/fixtures/NativeConditional.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_conditional)
        add_executable(LVUINativeViewValueTests tests/native_view_values.cpp "${_native_view_values}" "${_native_conditional}")
        target_include_directories(LVUINativeViewValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeViewValueTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeViewValues COMMAND LVUINativeViewValueTests)
        set_tests_properties(LVUI.NativeViewValues PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeComponents SOURCE tests/fixtures/NativeComponents.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_components)
        lvui_generate(NAME NativeComponentRetirement SOURCE tests/fixtures/NativeComponentRetirement.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_component_retirement)
        add_executable(LVUINativeComponentTests tests/native_components.cpp "${_native_components}" "${_native_component_retirement}")
        target_include_directories(LVUINativeComponentTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeComponentTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeComponents COMMAND LVUINativeComponentTests)
        set_tests_properties(LVUI.NativeComponents PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeScalarInputs SOURCE tests/fixtures/NativeScalarInputs.lvui
            INCLUDES NativeScalarInputFixture.h OUTPUT_VARIABLE _native_scalar_inputs)
        add_executable(LVUINativeScalarInputTests tests/native_scalar_inputs.cpp "${_native_scalar_inputs}")
        target_include_directories(LVUINativeScalarInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeScalarInputTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeScalarInputs COMMAND LVUINativeScalarInputTests)
        set_tests_properties(LVUI.NativeScalarInputs PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeLists SOURCE tests/fixtures/NativeLists.lvui
            INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _native_lists)
        add_executable(LVUINativeListTests tests/native_lists.cpp "${_native_lists}")
        target_include_directories(LVUINativeListTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_TEST_HEADERS}" "${LVUI_IIGUI_HEADER_DIR}")
        target_link_libraries(LVUINativeListTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeLists COMMAND LVUINativeListTests)
        set_tests_properties(LVUI.NativeLists PROPERTIES TIMEOUT 15)
    endif()
    add_executable(LVUINativeBackendFactoryTests tests/native_backend_factories.cpp)
    target_link_libraries(LVUINativeBackendFactoryTests PRIVATE LVUI::Native)
    foreach(_case creation snapshots failure sites)
        add_test(NAME LVUI.NativeBackendFactories.${_case} COMMAND LVUINativeBackendFactoryTests ${_case})
        set_tests_properties(LVUI.NativeBackendFactories.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeCallbackTests tests/native_callbacks.cpp)
    add_executable(LVUINativeCallbackValueTests tests/native_callback_values.cpp)
    add_executable(LVUINativeFunctionValueTests tests/native_function_values.cpp)
    add_executable(LVUINativeFunctionVariableTests tests/native_function_variables.cpp)
    target_link_libraries(LVUINativeFunctionVariableTests PRIVATE LVUI::Native)
    foreach(_case versions native rollback lifetime active threading bounds)
        add_test(NAME LVUI.NativeFunctionVariables.${_case} COMMAND LVUINativeFunctionVariableTests ${_case})
        set_tests_properties(LVUI.NativeFunctionVariables.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    target_link_libraries(LVUINativeFunctionValueTests PRIVATE LVUI::Native)
    foreach(_case results reactive lifetime threading snapshot snapshot_lifetime)
        add_test(NAME LVUI.NativeFunctionValues.${_case} COMMAND LVUINativeFunctionValueTests ${_case})
        set_tests_properties(LVUI.NativeFunctionValues.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    target_link_libraries(LVUINativeCallbackValueTests PRIVATE LVUI::Native)
    foreach(_case identity rollback active threading native_results returned_results)
        add_test(NAME LVUI.NativeCallbackValues.${_case} COMMAND LVUINativeCallbackValueTests ${_case})
        set_tests_properties(LVUI.NativeCallbackValues.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    target_link_libraries(LVUINativeCallbackTests PRIVATE LVUI::Native)
    foreach(_case reactive rollback lifetime typed)
        add_test(NAME LVUI.NativeCallbacks.${_case} COMMAND LVUINativeCallbackTests ${_case})
        set_tests_properties(LVUI.NativeCallbacks.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeBindingTests tests/native_bindings.cpp)
    target_link_libraries(LVUINativeBindingTests PRIVATE LVUI::Native)
    add_executable(LVUIPopupBindingCoreTests tests/popup_binding_core.cpp)
    target_link_libraries(LVUIPopupBindingCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Popup.CoreBinding COMMAND LVUIPopupBindingCoreTests)
    add_test(NAME LVUI.ListItem.HostFrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/list_item_host_framework_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.TableView.FrameworkBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_view_group_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME LVUI.TableHeader.PresentationBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/table_header_presentation_boundary.py" "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(LVUITableHeaderPresentationCoreTests tests/table_header_presentation_core.cpp)
    target_link_libraries(LVUITableHeaderPresentationCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableHeader.CorePresentation COMMAND LVUITableHeaderPresentationCoreTests)
    add_executable(LVUITableViewCoreBindingTests tests/table_view_binding_core.cpp)
    target_link_libraries(LVUITableViewCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TableView.CoreBinding COMMAND LVUITableViewCoreBindingTests)
    add_executable(LVUIListItemHostCoreBindingTests tests/list_item_host_binding_core.cpp)
    target_link_libraries(LVUIListItemHostCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListItem.CoreHostBinding COMMAND LVUIListItemHostCoreBindingTests)
    add_executable(LVUIListItemCoreBindingTests tests/list_item_binding_core.cpp)
    target_link_libraries(LVUIListItemCoreBindingTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.ListItem.CoreBinding COMMAND LVUIListItemCoreBindingTests)
    add_executable(LVUITextFieldMaterialBindingCoreTests tests/text_field_material_binding_core.cpp)
    target_link_libraries(LVUITextFieldMaterialBindingCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TextFieldMaterial.CoreBinding COMMAND LVUITextFieldMaterialBindingCoreTests)
    add_executable(LVUIVisualCoreTests tests/native_visual_core.cpp)
    target_link_libraries(LVUIVisualCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Visual.CoreContract COMMAND LVUIVisualCoreTests)
    add_executable(LVUISectionCoreTests tests/section_core.cpp)
    target_link_libraries(LVUISectionCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Section.CoreContract COMMAND LVUISectionCoreTests)
    add_executable(LVUIPanelBindingCoreTests tests/panel_binding_core.cpp)
    target_link_libraries(LVUIPanelBindingCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Panel.CoreBinding COMMAND LVUIPanelBindingCoreTests)
    add_executable(LVUITextBoundaryCoreTests tests/text_boundary_core.cpp)
    target_link_libraries(LVUITextBoundaryCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.TextInput.CoreBoundary COMMAND LVUITextBoundaryCoreTests)
    add_executable(LVUILayoutCoreTests tests/layout_core.cpp)
    target_link_libraries(LVUILayoutCoreTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.Layout.CoreContract COMMAND LVUILayoutCoreTests)
    add_executable(LVUINativeBindingDiagnosticsTests tests/native_binding_diagnostics.cpp)
    target_link_libraries(LVUINativeBindingDiagnosticsTests PRIVATE LVUI::Native)
    add_test(NAME LVUI.NativeBindings.Diagnostics COMMAND LVUINativeBindingDiagnosticsTests)
    set_tests_properties(LVUI.NativeBindings.Diagnostics PROPERTIES TIMEOUT 30)
    foreach(_case selective readonly readonly_members readonly_reentry readonly_owners readonly_references branches derived batches event_transactions fanout errors reentry lifetime threading values cycle subscription_rollback callbacks retire_effects)
        add_test(NAME LVUI.NativeBindings.${_case} COMMAND LVUINativeBindingTests ${_case})
        set_tests_properties(LVUI.NativeBindings.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIExpressionTests tests/expressions.cpp)
    target_link_libraries(LVUIExpressionTests PRIVATE LVUI::LVUI)
    foreach(_case precedence conditionals postfix annotations diagnostics limits aot)
        add_test(NAME LVUI.Expressions.${_case} COMMAND LVUIExpressionTests ${_case})
        set_tests_properties(LVUI.Expressions.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIStyleTokenTests tests/style_tokens.cpp)
    target_link_libraries(LVUIStyleTokenTests PRIVATE LVUI::LVUI)
    foreach(_case catalog aliases modes overrides css composite parser aot diagnostics selectors typed_syntax typed_values typed_diagnostics profile)
        add_test(NAME LVUI.StyleTokens.${_case} COMMAND LVUIStyleTokenTests ${_case})
        set_tests_properties(LVUI.StyleTokens.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUIIndependenceTests tests/independent_backend.cpp)
    target_link_libraries(LVUIIndependenceTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.IndependentBackend COMMAND LVUIIndependenceTests)
    set_tests_properties(LVUI.IndependentBackend PROPERTIES TIMEOUT 15)
    add_executable(LVUIExternalPaintFactoryTests tests/external_paint_factory.cpp)
    target_link_libraries(LVUIExternalPaintFactoryTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.external_paint_factory COMMAND LVUIExternalPaintFactoryTests)
    set_tests_properties(LVUI.external_paint_factory PROPERTIES TIMEOUT 30)
    add_executable(LVUICompilerTests tests/compiler.cpp)
    target_link_libraries(LVUICompilerTests PRIVATE LVUI::LVUI)
    add_executable(LVUIFigmaComponentContractTests tests/figma_component_contract.cpp)
    target_link_libraries(LVUIFigmaComponentContractTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.Figma.ComponentContract COMMAND LVUIFigmaComponentContractTests)
    set_tests_properties(LVUI.Figma.ComponentContract PROPERTIES TIMEOUT 10)
    foreach(_case generation validation actions escaping native_widget embedded_build markup application)
        add_test(NAME LVUI.AOT.${_case} COMMAND LVUICompilerTests ${_case})
        set_tests_properties(LVUI.AOT.${_case} PROPERTIES TIMEOUT 10)
    endforeach()
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    set(LVUI_TEST_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}" CACHE PATH "Android NDK for cross-configuration tests")
    if(LVUI_TEST_ANDROID_NDK AND EXISTS "${LVUI_TEST_ANDROID_NDK}/build/cmake/android.toolchain.cmake")
        add_test(NAME LVUI.AndroidApplication.Configuration COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/android_application_configure.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --ndk "${LVUI_TEST_ANDROID_NDK}")
        set_tests_properties(LVUI.AndroidApplication.Configuration PROPERTIES TIMEOUT 120)
    endif()
    if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
        execute_process(COMMAND xcrun --sdk iphoneos --show-sdk-path RESULT_VARIABLE _lvui_iphoneos_sdk OUTPUT_QUIET ERROR_QUIET)
        if(_lvui_iphoneos_sdk EQUAL 0)
            add_test(NAME LVUI.IOSApplication.Bundle COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ios_application_bundle.py" --build "${CMAKE_CURRENT_BINARY_DIR}")
            set_tests_properties(LVUI.IOSApplication.Bundle PROPERTIES TIMEOUT 180)
            add_test(NAME LVUI.IOSApplication.CrossConfiguration COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/ios_cross_configuration.py" --build "${CMAKE_CURRENT_BINARY_DIR}")
            set_tests_properties(LVUI.IOSApplication.CrossConfiguration PROPERTIES TIMEOUT 180)
            if(TARGET LVUINativeMultiWindowFixture)
                add_test(NAME LVUI.IOSApplication.Generated COMMAND "${Python3_EXECUTABLE}" -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/ios_generated_application_compile.py"
                    --build "${CMAKE_CURRENT_BINARY_DIR}" --gui-include "${LVUI_IIGUI_HEADER_DIR}"
                    --core-includes "$<JOIN:$<TARGET_PROPERTY:iiSDKCore::Contracts,INTERFACE_INCLUDE_DIRECTORIES>,|>")
                set_tests_properties(LVUI.IOSApplication.Generated PROPERTIES TIMEOUT 120)
            endif()
        endif()
    endif()
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/Widgets/NativeWidgets.h" AND
       EXISTS "${LVUI_IIGUI_HEADER_DIR}/iiGUI/Export.h")
        add_test(NAME LVUI.NativeCallableValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_callable_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeCallableValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeLayoutValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_layout_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeLayoutValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeReadonlyValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_readonly_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeReadonlyValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeObjectInputValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_object_input_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeObjectInputValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeMethodValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_method_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeMethodValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeMethodReferenceValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_method_reference_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeMethodReferenceValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeCallableFieldValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_callable_field_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeCallableFieldValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeFactoryCaptureValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_factory_capture_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeFactoryCaptureValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeInferredValueValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_inferred_value_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeInferredValueValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeWindowValidation COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_window_validation.py"
            --workspace @WORK@ --gui-include "${LVUI_IIGUI_TEST_HEADERS}" --gui-export-include "${LVUI_IIGUI_HEADER_DIR}" --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeWindowValidation PROPERTIES TIMEOUT 180)


    endif()
    add_test(NAME LVUI.NativeInstalled COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_install.py"
        --workspace @WORK@ --build "${CMAKE_CURRENT_BINARY_DIR}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --cmake "${CMAKE_COMMAND}")
    # This check builds relocated consumers for enabled modules and negative C++ probes.
    # Its cold compiler/package time is separate from UI or runtime latency.
    # Allow the complete package build under sanitizers and cold OS executable assessment.
    set_tests_properties(LVUI.NativeInstalled PROPERTIES TIMEOUT 900 RESOURCE_LOCK lvui_install_snapshot)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND TARGET LVUINativeDynamicWindowsFixture)
        add_test(NAME LVUI.NativeDeployment.MacOS COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_deployment.py"
            --workspace @WORK@ --build "${CMAKE_CURRENT_BINARY_DIR}" --cmake "${CMAKE_COMMAND}")
        set_tests_properties(LVUI.NativeDeployment.MacOS PROPERTIES TIMEOUT 300 RESOURCE_LOCK lvui_install_snapshot)
        add_test(NAME LVUI.NativeDeployment.PlatformConfigurations COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_deployment_platforms.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --cmake "${CMAKE_COMMAND}" --cxx "${CMAKE_CXX_COMPILER}")
        set_tests_properties(LVUI.NativeDeployment.PlatformConfigurations PROPERTIES TIMEOUT 120)
    endif()
    add_test(NAME LVUI.StyleTokens.Snapshot COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/update_style_tokens.py" --check)
    add_test(NAME LVUI.StyleTokens.Generator COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_style_token_generator.py")
    add_test(NAME LVUI.PackageWorkspace COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_workspace.py")
    add_test(NAME LVUI.PackageBoundary COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_boundary.py" "${CMAKE_CURRENT_BINARY_DIR}" @WORK@)
    # The complete installed catalog has thousands of assets. Copying and
    # removing it on an external volume can exceed the default minute.
    set_tests_properties(LVUI.PackageBoundary PROPERTIES TIMEOUT 600 RESOURCE_LOCK lvui_install_snapshot ENVIRONMENT "CMAKE_COMMAND=${CMAKE_COMMAND}")
    add_test(NAME LVUI.AOT.tooling COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot_tooling.py" $<TARGET_FILE:lvui-compile>
        @WORK@ "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunLVUICompiler.cmake")
    if(NOT MSVC)
        add_test(NAME LVUI.AOT.old_iigui_guard COMMAND ${_lvui_resource_runner} "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/old_iigui_guard.py" @WORK@ "$<TARGET_FILE:lvui-compile>" "${CMAKE_CXX_COMPILER}")
        # The guard launches multiple compiler preprocesses; cold external
        # workspace storage can delay process startup independently of the test.
        set_tests_properties(LVUI.AOT.old_iigui_guard PROPERTIES TIMEOUT 120)
    endif()
    add_test(NAME LVUI.KeywordSnapshot COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/update_keywords.py" --check)
    add_test(NAME LVUI.KeywordInventoryTests COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lvui_keywords.py")
    add_test(NAME LVUI.Figma.DefaultCatalog COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_figma_component_catalog.py")
    set_tests_properties(LVUI.Figma.DefaultCatalog PROPERTIES TIMEOUT 10)
    add_test(NAME LVUI.ComponentExample COMMAND lvui-check "${CMAKE_CURRENT_SOURCE_DIR}/docs/examples/ReservedComponents.lvui")
    add_test(NAME LVUI.CounterSyntax COMMAND lvui-check "${CMAKE_CURRENT_SOURCE_DIR}/docs/examples/Counter.lvui")
    add_executable(LVUILanguageTests tests/language.cpp)
    target_link_libraries(LVUILanguageTests PRIVATE LVUI::LVUI)
    foreach(_case catalog lexing views shadowing diagnostics limits)
        add_test(NAME LVUI.Language.${_case} COMMAND LVUILanguageTests ${_case})
        set_tests_properties(LVUI.Language.${_case} PROPERTIES TIMEOUT 10)
    endforeach()
    add_executable(LVUIParserTests tests/parser.cpp)
    target_link_libraries(LVUIParserTests PRIVATE LVUI::LVUI)
    foreach(_case syntax styles diagnostics limits)
        add_test(NAME LVUI.${_case} COMMAND LVUIParserTests ${_case})
        set_tests_properties(LVUI.${_case} PROPERTIES TIMEOUT 10)
    endforeach()
endif()
