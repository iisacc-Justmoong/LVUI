cmake_minimum_required(VERSION 3.31)
project(LVUI VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
include(cmake/SourceHeaders.cmake)
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
file(GLOB LVUI_BUILTIN_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/*.svg")
set(LVUI_BUILTIN_ICON_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons/LVUI/BuiltinIconAssets.h")
add_custom_command(OUTPUT "${LVUI_BUILTIN_ICON_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_icons.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons" --output "${LVUI_BUILTIN_ICON_HEADER}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_icons.py" ${LVUI_BUILTIN_ICON_FILES}
    VERBATIM)
add_custom_target(LVUIBuiltinIconAssets DEPENDS "${LVUI_BUILTIN_ICON_HEADER}")
set(LVUI_BUILTIN_HUE_RING_HEADER "${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons/LVUI/BuiltinHueRing.h")
add_custom_command(OUTPUT "${LVUI_BUILTIN_HUE_RING_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_raster.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/color-picker-hue-ring.png" --output "${LVUI_BUILTIN_HUE_RING_HEADER}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_raster.py" "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/color-picker-hue-ring.png"
    VERBATIM)
add_custom_target(LVUIBuiltinHueRing DEPENDS "${LVUI_BUILTIN_HUE_RING_HEADER}")
find_package(iiSDKCore 0.18.1 CONFIG REQUIRED COMPONENTS Contracts Observation)
include(cmake/LVUICoreObservation.cmake)
find_package(iiColorManager 0.1.1 CONFIG REQUIRED COMPONENTS Values)
add_library(LVUI STATIC src/Compiler/Parser.cpp src/Language/Language.cpp src/Compiler/NativeMarkup.cpp src/Compiler/Compiler.cpp src/Language/ConstantExpression.cpp src/Style/StyleTokens.cpp)
add_library(LVUI::LVUI ALIAS LVUI)
target_compile_features(LVUI PUBLIC cxx_std_23)
target_link_libraries(LVUI PUBLIC iiSDKCore::Contracts)
target_include_directories(LVUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUI LVUIBuiltinIconAssets LVUIBuiltinHueRing)
set_target_properties(LVUI PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
add_library(LVUINative STATIC src/Runtime/NativeBindings.cpp)
add_library(LVUI::Native ALIAS LVUINative)
set_target_properties(LVUINative PROPERTIES EXPORT_NAME Native CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_compile_features(LVUINative PUBLIC cxx_std_23)
target_link_libraries(LVUINative PUBLIC iiSDKCore::Observation iiColorManager::Values)
target_include_directories(LVUINative PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUINative LVUIBuiltinIconAssets LVUIBuiltinHueRing)
# Existing generated-view consumers of LVUI::LVUI keep linking after adopting
# native reactive output. Runtime-only consumers can select LVUI::Native.
target_link_libraries(LVUI PUBLIC LVUI::Native)
option(LVUI_BUILD_COLOR_ADAPTER "Build the optional iiColorManager surface adapter" OFF)
if(LVUI_BUILD_COLOR_ADAPTER)
    find_package(iiColorManager 0.1.1 CONFIG REQUIRED COMPONENTS iiColorManager)
    add_library(LVUIColor STATIC src/Color/ColorSelectionSurface.cpp)
    add_library(LVUI::Color ALIAS LVUIColor)
    set_target_properties(LVUIColor PROPERTIES EXPORT_NAME Color CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_features(LVUIColor PUBLIC cxx_std_23)
    target_link_libraries(LVUIColor PUBLIC LVUI::Native PRIVATE iiColorManager::iiColorManager)
    target_include_directories(LVUIColor PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    install(TARGETS LVUIColor EXPORT LVUIColorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT LVUIColorTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
endif()
option(LVUI_BUILD_RUNTIME "Build the optional activation work runtime" OFF)
if(LVUI_BUILD_RUNTIME)
    find_package(iiSDKCore 0.20 CONFIG REQUIRED COMPONENTS Runtime)
    add_library(LVUIRuntime STATIC src/Runtime/NativeWork.cpp)
    add_library(LVUI::Runtime ALIAS LVUIRuntime)
    set_target_properties(LVUIRuntime PROPERTIES EXPORT_NAME Runtime CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_features(LVUIRuntime PUBLIC cxx_std_23)
    target_link_libraries(LVUIRuntime PUBLIC LVUI::Native iiSDKCore::Runtime)
    target_include_directories(LVUIRuntime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    install(TARGETS LVUIRuntime EXPORT LVUIRuntimeTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT LVUIRuntimeTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
endif()
if(MSVC)
    target_compile_options(LVUI PRIVATE /W4 PUBLIC /Zc:__cplusplus /utf-8)
    target_compile_options(LVUINative PRIVATE /W4 PUBLIC /Zc:__cplusplus /utf-8)
else()
    target_compile_options(LVUI PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(LVUINative PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(lvui-check src/lvui-check.cpp)
target_link_libraries(lvui-check PRIVATE LVUI::LVUI)
set_target_properties(lvui-check PROPERTIES CXX_EXTENSIONS OFF)
add_executable(lvui-compile src/lvui-compile.cpp)
add_executable(LVUI::lvui-compile ALIAS lvui-compile)
target_link_libraries(lvui-compile PRIVATE LVUI::LVUI)
set_target_properties(lvui-compile PROPERTIES CXX_EXTENSIONS OFF)
# The compiler tools emit text and never execute native bindings. The legacy
# public library also links Native for generated-code consumers; do not retain
# its unused runtime dylibs in these independently relocated host tools.
foreach(_tool lvui-check lvui-compile)
    if(APPLE)
        target_link_options(${_tool} PRIVATE "LINKER:-dead_strip_dylibs")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_options(${_tool} PRIVATE "LINKER:--as-needed")
    endif()
endforeach()
include(cmake/LVUIAot.cmake)
option(LVUI_BUILD_STYLE_EXAMPLE "Build the LVUI style example with an application-supplied iiGUI host" OFF)
if(LVUI_BUILD_STYLE_EXAMPLE)
    add_subdirectory(example/StyleTokens)
endif()
option(LVUI_BUILD_TUTORIAL_EXAMPLES "Build the LVUI getting-started applications" OFF)
if(LVUI_BUILD_TUTORIAL_EXAMPLES)
    add_subdirectory(example/GettingStarted)
endif()
set(_package "${CMAKE_INSTALL_LIBDIR}/cmake/LVUI")
install(TARGETS LVUI LVUINative EXPORT LVUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS lvui-check RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS lvui-compile EXPORT LVUITargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
    src/Compiler/Document.h
    src/Controls/Card.h
    src/Controls/NativeCard.h
    src/Collections/Hierarchy.h
    src/Collections/List.h
    src/Collections/ListItem.h
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
    src/Collections/NativeTableHeader.h
    src/Collections/TableClipboard.h
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
    src/Collections/TableViewport.h
    src/Collections/NativeTable.h
    src/Collections/NativeListToolbar.h
    src/Collections/NativeHierarchyItem.h
    src/Collections/NativeHierarchyList.h
    src/Language/Language.h
    src/Compiler/Compiler.h
    src/Language/ConstantExpression.h
    src/Runtime/NativeBindings.h
    src/Runtime/NativeComponentInputs.h
    src/Runtime/NativeObjectInputs.h
    src/Runtime/NativeBackendFactories.h
    src/Runtime/NativeActivation.h
    src/Runtime/NativeWork.h
    src/Runtime/NativeValues.h
    src/Runtime/NativeMethods.h
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
    src/Media/Image.h
    src/Media/IconCatalog.h
    src/Media/NativeIconCache.h
    src/Controls/Tab.h
    src/Controls/NativeTab.h
    src/Controls/NativeTabBar.h
    src/Overlays/Menu.h
    src/Overlays/MenuItem.h
    src/Overlays/NativeMenuItem.h
    src/Overlays/NativeMenu.h
    src/Overlays/Popover.h
    src/Overlays/NativePopover.h
    src/Overlays/Modal.h
    src/Overlays/NativeModal.h
    src/Overlays/AlertButton.h
    src/Overlays/NativeAlertButton.h
    src/Overlays/Alert.h
    src/Overlays/NativeAlert.h
    src/Overlays/Sheet.h
    src/Overlays/NativeSheet.h
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
    src/Controls/NativeButton.h
    src/Controls/HelpButton.h
    src/Controls/DropdownButton.h
    src/Controls/NativeDropdownButton.h
    src/Controls/Divider.h
    src/Controls/NativeDivider.h
    src/Controls/ToggleSwitch.h
    src/Controls/NativeToggleSwitch.h
    src/Controls/CheckBox.h
    src/Controls/NativeCheckBox.h
    src/Controls/RadioButton.h
    src/Controls/RadioGroup.h
    src/Controls/NativeRadioButton.h
    src/Style/Material.h
    src/Style/NativeMaterial.h
    src/Navigation/Navigation.h
    src/Navigation/NavigationHistory.h
    src/Navigation/PageRouter.h
    src/Navigation/NativePageRouter.h
    src/Controls/AppCard.h
    src/Controls/NativeAppCard.h
    src/Controls/AppHeader.h
    src/Controls/NativeAppHeader.h
    src/Controls/EventListener.h
    src/Runtime/NativeEventListener.h
    src/Controls/Avatar.h
    src/Controls/NativeAvatar.h
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
    src/Controls/Link.h
    src/Controls/NativeLink.h
    src/Media/NativeImage.h
    src/Editing/NativeDocument.h
    src/Controls/Text.h
    src/Editing/TextInput.h
    src/Editing/InputField.h
    src/Editing/NativeTextBoundary.h
    src/Editing/NativeTextLayout.h
    src/Editing/NativeInputField.h
    src/Controls/ProgressBar.h
    src/Style/StyleTokens.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LVUI)
install(FILES "${LVUI_BUILTIN_ICON_HEADER}" "${LVUI_BUILTIN_HUE_RING_HEADER}" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/LVUI)
install(DIRECTORY resources/icons DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources)
install(FILES resources/images/color-picker-hue-ring.png DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/resources/images)
install(EXPORT LVUITargets NAMESPACE LVUI:: DESTINATION "${_package}")
configure_package_config_file(cmake/LVUIConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfig.cmake" INSTALL_DESTINATION "${_package}")
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/LVUIConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LVUIConfigVersion.cmake" DESTINATION "${_package}")
install(FILES cmake/LVUIAot.cmake cmake/RunLVUICompiler.cmake cmake/LVUICoreObservation.cmake DESTINATION "${_package}")
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI)
install(DIRECTORY docs DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI PATTERN .DS_Store EXCLUDE)
install(FILES catalog/lvui-keywords.json catalog/lvui-style-tokens.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/LVUI/catalog)
if(BUILD_TESTING)
    add_test(NAME LVUI.source_layout COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_layout.cmake)
    add_executable(LVUIOwnedIconCatalogTests tests/owned_icon_catalog.cpp)
    target_link_libraries(LVUIOwnedIconCatalogTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedIcons.Catalog COMMAND LVUIOwnedIconCatalogTests)
    add_test(NAME LVUI.OwnedIcons.Embedding COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_icon_embedding.py")
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
    add_test(NAME LVUI.OwnedListToolbar.Model COMMAND LVUIOwnedListToolbarTests)
    add_executable(LVUIOwnedHierarchyDragTests tests/owned_hierarchy_drag.cpp)
    target_link_libraries(LVUIOwnedHierarchyDragTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchy.Drag COMMAND LVUIOwnedHierarchyDragTests)
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
    add_executable(LVUIOwnedTableHeaderCompileTests tests/table_header_codegen.cpp)
    target_link_libraries(LVUIOwnedTableHeaderCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableHeader.Codegen COMMAND LVUIOwnedTableHeaderCompileTests)
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
    add_executable(LVUIOwnedTableHeaderTests tests/owned_table_header.cpp)
    target_link_libraries(LVUIOwnedTableHeaderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTableHeader.Model COMMAND LVUIOwnedTableHeaderTests)
    add_executable(LVUIOwnedCardTests tests/owned_card.cpp)
    target_link_libraries(LVUIOwnedCardTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCard.Model COMMAND LVUIOwnedCardTests)
    add_executable(LVUIOwnedListItemQuantityTests tests/owned_list_item_quantity.cpp)
    target_link_libraries(LVUIOwnedListItemQuantityTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Quantity COMMAND LVUIOwnedListItemQuantityTests)
    add_executable(LVUIOwnedListItemSelectorTests tests/owned_list_item_selector.cpp)
    target_link_libraries(LVUIOwnedListItemSelectorTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Selector COMMAND LVUIOwnedListItemSelectorTests)
    add_executable(LVUIOwnedListItemTests tests/owned_list_item.cpp)
    target_link_libraries(LVUIOwnedListItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListItem.Model COMMAND LVUIOwnedListItemTests)
    add_executable(LVUIOwnedListModelTests tests/owned_list_model.cpp)
    target_link_libraries(LVUIOwnedListModelTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.Model COMMAND LVUIOwnedListModelTests)
    add_executable(LVUIOwnedListViewportTests tests/owned_list_viewport.cpp)
    target_link_libraries(LVUIOwnedListViewportTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedList.Viewport COMMAND LVUIOwnedListViewportTests)
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
    add_executable(LVUIOwnedCodeDocumentTests tests/owned_code_document.cpp)
    target_link_libraries(LVUIOwnedCodeDocumentTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Model COMMAND LVUIOwnedCodeDocumentTests)
    add_executable(LVUIOwnedCodeEditorControllerTests tests/owned_code_editor_controller.cpp)
    target_link_libraries(LVUIOwnedCodeEditorControllerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Controller COMMAND LVUIOwnedCodeEditorControllerTests)
    add_executable(LVUIOwnedCodeViewportTests tests/owned_code_viewport.cpp)
    target_link_libraries(LVUIOwnedCodeViewportTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCodeDocument.Viewport COMMAND LVUIOwnedCodeViewportTests)
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
    add_test(NAME LVUI.OwnedToolbarButton.Model COMMAND LVUIOwnedToolbarButtonTests)
    add_executable(LVUIOwnedToolbarButtonCompileTests tests/toolbar_button_codegen.cpp)
    target_link_libraries(LVUIOwnedToolbarButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToolbarButton.Codegen COMMAND LVUIOwnedToolbarButtonCompileTests)
    add_executable(LVUIOwnedListFooterTests tests/owned_list_footer.cpp)
    target_link_libraries(LVUIOwnedListFooterTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListFooter.Model COMMAND LVUIOwnedListFooterTests)
    add_executable(LVUIOwnedListFooterCompileTests tests/list_footer_codegen.cpp)
    target_link_libraries(LVUIOwnedListFooterCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedListFooter.Codegen COMMAND LVUIOwnedListFooterCompileTests)
    add_executable(LVUIOwnedHierarchyToolbarTests tests/owned_hierarchy_toolbar.cpp)
    target_link_libraries(LVUIOwnedHierarchyToolbarTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyToolbar.Model COMMAND LVUIOwnedHierarchyToolbarTests)
    add_executable(LVUIOwnedHierarchyToolbarCompileTests tests/hierarchy_toolbar_codegen.cpp)
    target_link_libraries(LVUIOwnedHierarchyToolbarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyToolbar.Codegen COMMAND LVUIOwnedHierarchyToolbarCompileTests)
    add_executable(LVUIOwnedTableViewportTests tests/owned_table_viewport.cpp)
    target_link_libraries(LVUIOwnedTableViewportTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Viewport COMMAND LVUIOwnedTableViewportTests)
    add_executable(LVUIOwnedTableTests tests/owned_table.cpp)
    target_link_libraries(LVUIOwnedTableTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTable.Model COMMAND LVUIOwnedTableTests)
    add_executable(LVUIOwnedHierarchyTests tests/owned_hierarchy.cpp)
    target_link_libraries(LVUIOwnedHierarchyTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchy.Model COMMAND LVUIOwnedHierarchyTests)
    add_executable(LVUIOwnedHierarchyItemTests tests/owned_hierarchy_item.cpp)
    target_link_libraries(LVUIOwnedHierarchyItemTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyItem.Model COMMAND LVUIOwnedHierarchyItemTests)
    add_executable(LVUIOwnedHierarchyViewportTests tests/owned_hierarchy_viewport.cpp)
    target_link_libraries(LVUIOwnedHierarchyViewportTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchy.Viewport COMMAND LVUIOwnedHierarchyViewportTests)
    add_executable(LVUIHierarchyListCompileTests tests/hierarchy_list_codegen.cpp)
    target_link_libraries(LVUIHierarchyListCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyList.Codegen COMMAND LVUIHierarchyListCompileTests)
    add_executable(LVUIHierarchyItemCompileTests tests/hierarchy_item_codegen.cpp)
    target_link_libraries(LVUIHierarchyItemCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHierarchyItem.Codegen COMMAND LVUIHierarchyItemCompileTests)
    add_executable(LVUIOwnedColorPickerLayoutTests tests/owned_color_picker_layout.cpp)
    target_link_libraries(LVUIOwnedColorPickerLayoutTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedColorPicker.Layout COMMAND LVUIOwnedColorPickerLayoutTests)
    if(LVUI_BUILD_COLOR_ADAPTER)
        add_executable(LVUIOwnedColorSurfaceTests tests/owned_color_surface.cpp)
        target_link_libraries(LVUIOwnedColorSurfaceTests PRIVATE LVUI::Color)
        add_test(NAME LVUI.OwnedColorSurface.Model COMMAND LVUIOwnedColorSurfaceTests)
        add_executable(LVUIOwnedColorPickerSessionTests tests/owned_color_picker_session.cpp)
        target_link_libraries(LVUIOwnedColorPickerSessionTests PRIVATE LVUI::Color)
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
    add_test(NAME LVUI.OwnedModal.Model COMMAND LVUIOwnedModalTests)
    add_executable(LVUIOwnedAlertButtonTests tests/owned_alert_button.cpp)
    target_link_libraries(LVUIOwnedAlertButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlertButton.Model COMMAND LVUIOwnedAlertButtonTests)
    add_executable(LVUIOwnedAlertTests tests/owned_alert.cpp)
    target_link_libraries(LVUIOwnedAlertTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAlert.Model COMMAND LVUIOwnedAlertTests)
    add_executable(LVUIOwnedSheetTests tests/owned_sheet.cpp)
    target_link_libraries(LVUIOwnedSheetTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSheet.Model COMMAND LVUIOwnedSheetTests)
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
    add_executable(LVUIOwnedProgressTests tests/progress_bar_owned.cpp)
    target_compile_features(LVUIOwnedProgressTests PRIVATE cxx_std_23)
    target_include_directories(LVUIOwnedProgressTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_BINARY_DIR}/include")
    add_test(NAME LVUI.OwnedProgress.Model COMMAND LVUIOwnedProgressTests)
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
    target_link_libraries(LVUIOwnedButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedButton.Model COMMAND LVUIOwnedButtonTests)
    add_executable(LVUIOwnedDividerTests tests/owned_divider.cpp)
    target_link_libraries(LVUIOwnedDividerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedDivider.Model COMMAND LVUIOwnedDividerTests)
    add_executable(LVUIOwnedToggleSwitchTests tests/owned_toggle_switch.cpp)
    target_link_libraries(LVUIOwnedToggleSwitchTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedToggleSwitch.Model COMMAND LVUIOwnedToggleSwitchTests)
    add_executable(LVUIOwnedCheckBoxTests tests/owned_check_box.cpp)
    target_link_libraries(LVUIOwnedCheckBoxTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCheckBox.Model COMMAND LVUIOwnedCheckBoxTests)
    add_executable(LVUICheckBoxCompileTests tests/check_box.cpp)
    target_link_libraries(LVUICheckBoxCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedCheckBox.Codegen COMMAND LVUICheckBoxCompileTests)
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
    add_executable(LVUIOwnedAppHeaderTests tests/owned_app_header.cpp)
    target_link_libraries(LVUIOwnedAppHeaderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAppHeader.Model COMMAND LVUIOwnedAppHeaderTests)
    add_executable(LVUIAppHeaderCompileTests tests/app_header.cpp)
    target_link_libraries(LVUIAppHeaderCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAppHeader.Codegen COMMAND LVUIAppHeaderCompileTests)
    add_executable(LVUIOwnedEventListenerTests tests/owned_event_listener.cpp)
    target_link_libraries(LVUIOwnedEventListenerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedEventListener.Model COMMAND LVUIOwnedEventListenerTests)
    add_executable(LVUIEventListenerCompileTests tests/event_listener.cpp)
    target_link_libraries(LVUIEventListenerCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedEventListener.Codegen COMMAND LVUIEventListenerCompileTests)
    add_executable(LVUIOwnedAvatarTests tests/owned_avatar.cpp)
    target_link_libraries(LVUIOwnedAvatarTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAvatar.Model COMMAND LVUIOwnedAvatarTests)
    add_executable(LVUIAvatarCompileTests tests/avatar.cpp)
    target_link_libraries(LVUIAvatarCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedAvatar.Codegen COMMAND LVUIAvatarCompileTests)
    add_executable(LVUIOwnedTooltipTests tests/owned_tooltip.cpp)
    target_link_libraries(LVUIOwnedTooltipTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Model COMMAND LVUIOwnedTooltipTests)
    add_executable(LVUIOwnedTooltipControllerTests tests/owned_tooltip_controller.cpp)
    target_link_libraries(LVUIOwnedTooltipControllerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Controller COMMAND LVUIOwnedTooltipControllerTests)
    add_executable(LVUITooltipCompileTests tests/tooltip.cpp)
    target_link_libraries(LVUITooltipCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTooltip.Codegen COMMAND LVUITooltipCompileTests)
    add_executable(LVUIOwnedSliderTests tests/owned_slider.cpp)
    target_link_libraries(LVUIOwnedSliderTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSlider.Model COMMAND LVUIOwnedSliderTests)
    add_executable(LVUIOwnedTextInputTests tests/owned_text_input.cpp)
    target_link_libraries(LVUIOwnedTextInputTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedTextInput.Model COMMAND LVUIOwnedTextInputTests)
    add_executable(LVUIOwnedInputFieldTests tests/owned_input_field.cpp)
    target_link_libraries(LVUIOwnedInputFieldTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedInputField.Configuration COMMAND LVUIOwnedInputFieldTests)
    add_executable(LVUIInputFieldCompileTests tests/input_field.cpp)
    target_link_libraries(LVUIInputFieldCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedInputField.Codegen COMMAND LVUIInputFieldCompileTests)
    add_executable(LVUIOwnedStepperTests tests/owned_stepper.cpp)
    target_link_libraries(LVUIOwnedStepperTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedStepper.Model COMMAND LVUIOwnedStepperTests)
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
    add_executable(LVUISegmentedControlCompileTests tests/segmented_control.cpp)
    target_link_libraries(LVUISegmentedControlCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedSegmentedControl.Codegen COMMAND LVUISegmentedControlCompileTests)
    add_executable(LVUIOwnedHelpButtonTests tests/owned_help_button.cpp)
    target_link_libraries(LVUIOwnedHelpButtonTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHelpButton.Model COMMAND LVUIOwnedHelpButtonTests)
    add_executable(LVUIHelpButtonCompileTests tests/help_button.cpp)
    target_link_libraries(LVUIHelpButtonCompileTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedHelpButton.Codegen COMMAND LVUIHelpButtonCompileTests)
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
    add_executable(LVUIOwnedImageTests tests/owned_image.cpp)
    target_link_libraries(LVUIOwnedImageTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedImage.Model COMMAND LVUIOwnedImageTests)
    add_executable(LVUINativeViewCompileTests tests/native_views.cpp)
    target_link_libraries(LVUINativeViewCompileTests PRIVATE LVUI::LVUI)
    foreach(_case frameworks bindings empty isolation diagnostics)
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
    foreach(_case private failures canonical reentry initial isolation)
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
    add_executable(LVUINativeCollectionNavigationTests tests/native_collection_navigation.cpp)
    target_link_libraries(LVUINativeCollectionNavigationTests PRIVATE LVUI::Native)
    foreach(_case navigation selection projection eligibility lifetime modes restore)
        add_test(NAME LVUI.NativeCollectionNavigation.${_case} COMMAND LVUINativeCollectionNavigationTests ${_case})
        set_tests_properties(LVUI.NativeCollectionNavigation.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(LVUINativeVirtualCollectionTests tests/native_virtual_collection.cpp)
    target_link_libraries(LVUINativeVirtualCollectionTests PRIVATE LVUI::Native)
    add_executable(LVUINativeMeasuredCollectionTests tests/native_measured_collection.cpp)
    target_link_libraries(LVUINativeMeasuredCollectionTests PRIVATE LVUI::Native)
    foreach(_case index measured grid anchor lifetime diagnostics ambient sparse)
        add_test(NAME LVUI.NativeMeasuredCollection.${_case} COMMAND LVUINativeMeasuredCollectionTests ${_case})
        set_tests_properties(LVUI.NativeMeasuredCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    foreach(_case vertical grid anchor lifetime diagnostics indexed reveal)
        add_test(NAME LVUI.NativeVirtualCollection.${_case} COMMAND LVUINativeVirtualCollectionTests ${_case})
        set_tests_properties(LVUI.NativeVirtualCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    foreach(_case identity rollback range lifetime diagnostics sparse)
        add_test(NAME LVUI.NativeCollection.${_case} COMMAND LVUINativeCollectionTests ${_case})
        set_tests_properties(LVUI.NativeCollection.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
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
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/Widgets/NativeWidgets.h" AND
       EXISTS "${LVUI_IIGUI_SOURCE_DIR}/build/install/include/iiGUI/Export.h")
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
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedProgressCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedImage SOURCE tests/fixtures/NativeOwnedImage.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_generated)
        lvui_generate(NAME NativeOwnedImageAdvanced SOURCE tests/fixtures/NativeOwnedImageAdvanced.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_advanced)
        lvui_generate(NAME NativeOwnedImageStatic SOURCE tests/fixtures/NativeOwnedImageStatic.lvui
            OUTPUT_VARIABLE _owned_image_static)
        add_library(LVUIOwnedImageCompile OBJECT tests/native_owned_image_compile.cpp "${_owned_image_generated}" "${_owned_image_advanced}" "${_owned_image_static}")
        target_include_directories(LVUIOwnedImageCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedImageCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedButton SOURCE tests/fixtures/NativeOwnedButton.lvui
            INCLUDES NativeOwnedButtonFixture.h OUTPUT_VARIABLE _owned_button_generated)
        add_library(LVUIOwnedButtonCompile OBJECT tests/native_owned_button_compile.cpp "${_owned_button_generated}")
        target_include_directories(LVUIOwnedButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedDivider SOURCE tests/fixtures/NativeOwnedDivider.lvui
            INCLUDES NativeOwnedDividerFixture.h OUTPUT_VARIABLE _owned_divider_generated)
        add_library(LVUIOwnedDividerCompile OBJECT tests/native_owned_divider_compile.cpp "${_owned_divider_generated}")
        target_include_directories(LVUIOwnedDividerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedDividerCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedToggleSwitch SOURCE tests/fixtures/NativeOwnedToggleSwitch.lvui
            INCLUDES NativeOwnedToggleSwitchFixture.h OUTPUT_VARIABLE _owned_toggle_generated)
        add_library(LVUIOwnedToggleSwitchCompile OBJECT tests/native_owned_toggle_switch_compile.cpp "${_owned_toggle_generated}")
        target_include_directories(LVUIOwnedToggleSwitchCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedToggleSwitchCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedCheckBox SOURCE tests/fixtures/NativeOwnedCheckBox.lvui
            INCLUDES NativeOwnedCheckBoxFixture.h OUTPUT_VARIABLE _owned_checkbox_generated)
        add_library(LVUIOwnedCheckBoxCompile OBJECT tests/native_owned_check_box_compile.cpp "${_owned_checkbox_generated}")
        target_include_directories(LVUIOwnedCheckBoxCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedCheckBoxCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedRadioButton SOURCE tests/fixtures/NativeOwnedRadioButton.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_generated)
        lvui_generate(NAME NativeOwnedRadioRows SOURCE tests/fixtures/NativeOwnedRadioRows.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_rows_generated)
        add_library(LVUIOwnedRadioButtonCompile OBJECT tests/native_owned_radio_button_compile.cpp "${_owned_radio_generated}" "${_owned_radio_rows_generated}")
        target_include_directories(LVUIOwnedRadioButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedRadioButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedMaterial SOURCE tests/fixtures/NativeOwnedMaterial.lvui
            INCLUDES NativeOwnedMaterialFixture.h OUTPUT_VARIABLE _owned_material_generated)
        add_library(LVUIOwnedMaterialCompile OBJECT tests/native_owned_material_compile.cpp "${_owned_material_generated}")
        target_include_directories(LVUIOwnedMaterialCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedMaterialCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedLink SOURCE tests/fixtures/NativeOwnedLink.lvui
            INCLUDES NativeOwnedLinkFixture.h OUTPUT_VARIABLE _owned_link_generated)
        lvui_generate(NAME NativeOwnedLinkMethods SOURCE tests/fixtures/NativeOwnedLinkMethods.lvui
            INCLUDES NativeOwnedLinkMethodsFixture.h OUTPUT_VARIABLE _owned_link_methods_generated)
        add_library(LVUIOwnedLinkCompile OBJECT tests/native_owned_link_compile.cpp "${_owned_link_generated}" "${_owned_link_methods_generated}")
        target_include_directories(LVUIOwnedLinkCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedLinkCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedPageRouter SOURCE tests/fixtures/NativeOwnedPageRouter.lvui
            INCLUDES NativeOwnedPageRouterFixture.h OUTPUT_VARIABLE _owned_page_router_generated)
        add_library(LVUIOwnedPageRouterCompile OBJECT tests/native_owned_page_router_compile.cpp "${_owned_page_router_generated}")
        target_include_directories(LVUIOwnedPageRouterCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedPageRouterCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppCard SOURCE tests/fixtures/NativeOwnedAppCard.lvui
            INCLUDES NativeOwnedAppCardFixture.h OUTPUT_VARIABLE _owned_app_card_generated)
        add_library(LVUIOwnedAppCardCompile OBJECT tests/native_owned_app_card_compile.cpp "${_owned_app_card_generated}")
        target_include_directories(LVUIOwnedAppCardCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAppCardCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppHeader SOURCE tests/fixtures/NativeOwnedAppHeader.lvui
            INCLUDES NativeOwnedAppHeaderFixture.h OUTPUT_VARIABLE _owned_app_header_generated)
        add_library(LVUIOwnedAppHeaderCompile OBJECT tests/native_owned_app_header_compile.cpp "${_owned_app_header_generated}")
        target_include_directories(LVUIOwnedAppHeaderCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAppHeaderCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedEventListener SOURCE tests/fixtures/NativeOwnedEventListener.lvui
            INCLUDES NativeOwnedEventListenerFixture.h OUTPUT_VARIABLE _owned_event_listener_generated)
        add_library(LVUIOwnedEventListenerCompile OBJECT tests/native_owned_event_listener_compile.cpp "${_owned_event_listener_generated}")
        target_include_directories(LVUIOwnedEventListenerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedEventListenerCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAvatar SOURCE tests/fixtures/NativeOwnedAvatar.lvui
            INCLUDES NativeOwnedAvatarFixture.h OUTPUT_VARIABLE _owned_avatar_generated)
        add_library(LVUIOwnedAvatarCompile OBJECT tests/native_owned_avatar_compile.cpp "${_owned_avatar_generated}")
        target_include_directories(LVUIOwnedAvatarCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAvatarCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeReactive SOURCE tests/fixtures/NativeReactive.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_generated)
        add_executable(LVUINativeGeneratedTests tests/native_generated.cpp "${_native_generated}")
        target_include_directories(LVUINativeGeneratedTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
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
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeNestedTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeNested COMMAND LVUINativeNestedTests)
        set_tests_properties(LVUI.NativeNested PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeViewValues SOURCE tests/fixtures/NativeViewValues.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_view_values)
        lvui_generate(NAME NativeConditional SOURCE tests/fixtures/NativeConditional.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_conditional)
        add_executable(LVUINativeViewValueTests tests/native_view_values.cpp "${_native_view_values}" "${_native_conditional}")
        target_include_directories(LVUINativeViewValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeViewValueTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeViewValues COMMAND LVUINativeViewValueTests)
        set_tests_properties(LVUI.NativeViewValues PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeComponents SOURCE tests/fixtures/NativeComponents.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_components)
        lvui_generate(NAME NativeComponentRetirement SOURCE tests/fixtures/NativeComponentRetirement.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_component_retirement)
        add_executable(LVUINativeComponentTests tests/native_components.cpp "${_native_components}" "${_native_component_retirement}")
        target_include_directories(LVUINativeComponentTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeComponentTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeComponents COMMAND LVUINativeComponentTests)
        set_tests_properties(LVUI.NativeComponents PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeScalarInputs SOURCE tests/fixtures/NativeScalarInputs.lvui
            INCLUDES NativeScalarInputFixture.h OUTPUT_VARIABLE _native_scalar_inputs)
        add_executable(LVUINativeScalarInputTests tests/native_scalar_inputs.cpp "${_native_scalar_inputs}")
        target_include_directories(LVUINativeScalarInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeScalarInputTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeScalarInputs COMMAND LVUINativeScalarInputTests)
        set_tests_properties(LVUI.NativeScalarInputs PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeLists SOURCE tests/fixtures/NativeLists.lvui
            INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _native_lists)
        add_executable(LVUINativeListTests tests/native_lists.cpp "${_native_lists}")
        target_include_directories(LVUINativeListTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/include" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
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
    foreach(_case selective readonly readonly_members readonly_reentry readonly_owners readonly_references branches derived batches fanout errors reentry lifetime threading values cycle subscription_rollback callbacks)
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
    add_executable(LVUICompilerTests tests/compiler.cpp)
    target_link_libraries(LVUICompilerTests PRIVATE LVUI::LVUI)
    foreach(_case generation validation actions escaping native_widget embedded_build markup)
        add_test(NAME LVUI.AOT.${_case} COMMAND LVUICompilerTests ${_case})
        set_tests_properties(LVUI.AOT.${_case} PROPERTIES TIMEOUT 10)
    endforeach()
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/Widgets/NativeWidgets.h" AND
       EXISTS "${LVUI_IIGUI_SOURCE_DIR}/build/install/include/iiGUI/Export.h")
        add_test(NAME LVUI.NativeCallableValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_callable_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeCallableValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeLayoutValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_layout_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeLayoutValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeReadonlyValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_readonly_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeReadonlyValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeObjectInputValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_object_input_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeObjectInputValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeMethodValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_method_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeMethodValidation PROPERTIES TIMEOUT 120)
        add_test(NAME LVUI.NativeMethodReferenceValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_method_reference_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeMethodReferenceValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeCallableFieldValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_callable_field_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeCallableFieldValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeFactoryCaptureValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_factory_capture_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeFactoryCaptureValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeInferredValueValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_inferred_value_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeInferredValueValidation PROPERTIES TIMEOUT 180)
        add_test(NAME LVUI.NativeWindowValidation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_window_validation.py"
            --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
            --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --gui "${LVUI_IIGUI_SOURCE_DIR}")
        set_tests_properties(LVUI.NativeWindowValidation PROPERTIES TIMEOUT 180)


    endif()
    add_test(NAME LVUI.NativeInstalled COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_install.py"
        --build "${CMAKE_CURRENT_BINARY_DIR}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --cmake "${CMAKE_COMMAND}")
    # This check builds three fresh relocated consumers and negative C++ probes.
    # Its cold compiler/package time is separate from UI or runtime latency.
    # Allow the complete package build under sanitizers and cold OS executable assessment.
    set_tests_properties(LVUI.NativeInstalled PROPERTIES TIMEOUT 360)
    add_test(NAME LVUI.StyleTokens.Snapshot COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/update_style_tokens.py" --check)
    add_test(NAME LVUI.StyleTokens.Generator COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_style_token_generator.py")
    add_test(NAME LVUI.PackageBoundary COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_package_boundary.py" "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(LVUI.PackageBoundary PROPERTIES TIMEOUT 60 ENVIRONMENT "CMAKE_COMMAND=${CMAKE_COMMAND}")
    add_test(NAME LVUI.AOT.tooling COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/aot_tooling.py" $<TARGET_FILE:lvui-compile>
        "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunLVUICompiler.cmake")
    add_test(NAME LVUI.KeywordSnapshot COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/update_keywords.py" --check)
    add_test(NAME LVUI.KeywordInventoryTests COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_lvui_keywords.py")
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
