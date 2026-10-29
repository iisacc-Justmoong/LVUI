cmake_minimum_required(VERSION 3.31)
project(LVUI VERSION 0.1.0 LANGUAGES CXX)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)
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
add_library(LVUI STATIC src/LVUI/Parser.cpp src/LVUI/Language.cpp src/LVUI/NativeMarkup.cpp src/LVUI/Compiler.cpp src/LVUI/ConstantExpression.cpp src/LVUI/StyleTokens.cpp)
add_library(LVUI::LVUI ALIAS LVUI)
target_compile_features(LVUI PUBLIC cxx_std_23)
target_link_libraries(LVUI PUBLIC iiSDKCore::Contracts)
target_include_directories(LVUI PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUI LVUIBuiltinIconAssets LVUIBuiltinHueRing)
set_target_properties(LVUI PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
add_library(LVUINative STATIC src/LVUI/NativeBindings.cpp)
add_library(LVUI::Native ALIAS LVUINative)
set_target_properties(LVUINative PROPERTIES EXPORT_NAME Native CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_compile_features(LVUINative PUBLIC cxx_std_23)
target_link_libraries(LVUINative PUBLIC iiSDKCore::Observation iiColorManager::Values)
target_include_directories(LVUINative PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lvui-generated-icons>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
add_dependencies(LVUINative LVUIBuiltinIconAssets LVUIBuiltinHueRing)
# Existing generated-view consumers of LVUI::LVUI keep linking after adopting
# native reactive output. Runtime-only consumers can select LVUI::Native.
target_link_libraries(LVUI PUBLIC LVUI::Native)
option(LVUI_BUILD_COLOR_ADAPTER "Build the optional iiColorManager surface adapter" OFF)
if(LVUI_BUILD_COLOR_ADAPTER)
    find_package(iiColorManager 0.1.1 CONFIG REQUIRED COMPONENTS iiColorManager)
    add_library(LVUIColor STATIC src/LVUI/ColorSelectionSurface.cpp)
    add_library(LVUI::Color ALIAS LVUIColor)
    set_target_properties(LVUIColor PROPERTIES EXPORT_NAME Color CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_features(LVUIColor PUBLIC cxx_std_23)
    target_link_libraries(LVUIColor PUBLIC LVUI::Native PRIVATE iiColorManager::iiColorManager)
    target_include_directories(LVUIColor PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    install(TARGETS LVUIColor EXPORT LVUIColorTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(EXPORT LVUIColorTargets NAMESPACE LVUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LVUI)
endif()
option(LVUI_BUILD_RUNTIME "Build the optional activation work runtime" OFF)
if(LVUI_BUILD_RUNTIME)
    find_package(iiSDKCore 0.20 CONFIG REQUIRED COMPONENTS Runtime)
    add_library(LVUIRuntime STATIC src/LVUI/NativeWork.cpp)
    add_library(LVUI::Runtime ALIAS LVUIRuntime)
    set_target_properties(LVUIRuntime PROPERTIES EXPORT_NAME Runtime CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    target_compile_features(LVUIRuntime PUBLIC cxx_std_23)
    target_link_libraries(LVUIRuntime PUBLIC LVUI::Native iiSDKCore::Runtime)
    target_include_directories(LVUIRuntime PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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
set(_package "${CMAKE_INSTALL_LIBDIR}/cmake/LVUI")
install(TARGETS LVUI LVUINative EXPORT LVUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS lvui-check RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS lvui-compile EXPORT LVUITargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
    src/LVUI/Document.h
    src/LVUI/Card.h
    src/LVUI/NativeCard.h
    src/LVUI/Hierarchy.h
    src/LVUI/List.h
    src/LVUI/ListItem.h
    src/LVUI/ListItemQuantity.h
    src/LVUI/ListItemSelector.h
    src/LVUI/NativeListItemSelector.h
    src/LVUI/NativeVisual.h
    src/LVUI/NativeListItemDrawing.h
    src/LVUI/NativeListItem.h
    src/LVUI/NativeListItemComposite.h
    src/LVUI/ListViewport.h
    src/LVUI/PanelRegions.h
    src/LVUI/ListPanel.h
    src/LVUI/NativeListView.h
    src/LVUI/NativeListPanel.h
    src/LVUI/HierarchyPanel.h
    src/LVUI/NativeHierarchyPanel.h
    src/LVUI/HierarchyDrag.h
    src/LVUI/HierarchyItem.h
    src/LVUI/HierarchyItemProjection.h
    src/LVUI/HierarchyViewport.h
    src/LVUI/ListToolbar.h
    src/LVUI/Table.h
    src/LVUI/TableRowView.h
    src/LVUI/NativeTableRow.h
    src/LVUI/TableCellItem.h
    src/LVUI/NativeTableCellItem.h
    src/LVUI/TableHeader.h
    src/LVUI/NativeTableHeader.h
    src/LVUI/TableClipboard.h
    src/LVUI/TableEditing.h
    src/LVUI/TableSorting.h
    src/LVUI/TableStructure.h
    src/LVUI/CodeDocument.h
    src/LVUI/CodeViewport.h
    src/LVUI/CodeComposition.h
    src/LVUI/CodeEditorController.h
    src/LVUI/CodeEditorSession.h
    src/LVUI/NativeCodeEditor.h
    src/LVUI/ToolbarButton.h
    src/LVUI/NativeToolbarButton.h
    src/LVUI/ListFooter.h
    src/LVUI/NativeListFooter.h
    src/LVUI/HierarchyToolbar.h
    src/LVUI/HierarchyToolbarLayout.h
    src/LVUI/NativeHierarchyToolbar.h
    src/LVUI/TableAppearance.h
    src/LVUI/TableViewport.h
    src/LVUI/NativeTable.h
    src/LVUI/NativeListToolbar.h
    src/LVUI/NativeHierarchyItem.h
    src/LVUI/NativeHierarchyList.h
    src/LVUI/Language.h
    src/LVUI/Compiler.h
    src/LVUI/ConstantExpression.h
    src/LVUI/NativeBindings.h
    src/LVUI/NativeComponentInputs.h
    src/LVUI/NativeObjectInputs.h
    src/LVUI/NativeBackendFactories.h
    src/LVUI/NativeActivation.h
    src/LVUI/NativeWork.h
    src/LVUI/NativeValues.h
    src/LVUI/NativeMethods.h
    src/LVUI/NativeInferredValues.h
    src/LVUI/NativeColors.h
    src/LVUI/NativeWidgetColors.h
    src/LVUI/NativeWidgetLayout.h
    src/LVUI/NativeLayoutValues.h
    src/LVUI/NativeWindowValues.h
    src/LVUI/NativeFunctionVariable.h
    src/LVUI/NativeCollection.h
    src/LVUI/NativeCollectionNavigation.h
    src/LVUI/NativeCollectionWidgets.h
    src/LVUI/NativeVirtualCollection.h
    src/LVUI/NativeLineIndex.h
    src/LVUI/NativeMeasuredCollection.h
    src/LVUI/Motion.h
    src/LVUI/Image.h
    src/LVUI/IconCatalog.h
    src/LVUI/NativeIconCache.h
    src/LVUI/Tab.h
    src/LVUI/NativeTab.h
    src/LVUI/NativeTabBar.h
    src/LVUI/Menu.h
    src/LVUI/MenuItem.h
    src/LVUI/NativeMenuItem.h
    src/LVUI/NativeMenu.h
    src/LVUI/Popover.h
    src/LVUI/NativePopover.h
    src/LVUI/Modal.h
    src/LVUI/NativeModal.h
    src/LVUI/AlertButton.h
    src/LVUI/NativeAlertButton.h
    src/LVUI/Alert.h
    src/LVUI/NativeAlert.h
    src/LVUI/Sheet.h
    src/LVUI/NativeSheet.h
    src/LVUI/ColorPickerButton.h
    src/LVUI/ColorPicker.h
    src/LVUI/ColorPickerLayout.h
    src/LVUI/ColorSource.h
    src/LVUI/ColorSelection.h
    src/LVUI/ColorSurface.h
    src/LVUI/ColorChannels.h
    src/LVUI/ColorChannelSurface.h
    src/LVUI/NativeColorChannel.h
    src/LVUI/ColorSelectionSurface.h
    src/LVUI/ColorPickerSession.h
    src/LVUI/NativeColorPickerValues.h
    src/LVUI/NativeColorPicker.h
    src/LVUI/NativeColorSurface.h
    src/LVUI/NativeColorPickerButton.h
    src/LVUI/Button.h
    src/LVUI/NativeButton.h
    src/LVUI/HelpButton.h
    src/LVUI/DropdownButton.h
    src/LVUI/NativeDropdownButton.h
    src/LVUI/Divider.h
    src/LVUI/NativeDivider.h
    src/LVUI/ToggleSwitch.h
    src/LVUI/NativeToggleSwitch.h
    src/LVUI/CheckBox.h
    src/LVUI/NativeCheckBox.h
    src/LVUI/RadioButton.h
    src/LVUI/RadioGroup.h
    src/LVUI/NativeRadioButton.h
    src/LVUI/Material.h
    src/LVUI/NativeMaterial.h
    src/LVUI/Navigation.h
    src/LVUI/NavigationHistory.h
    src/LVUI/PageRouter.h
    src/LVUI/NativePageRouter.h
    src/LVUI/AppCard.h
    src/LVUI/NativeAppCard.h
    src/LVUI/AppHeader.h
    src/LVUI/NativeAppHeader.h
    src/LVUI/EventListener.h
    src/LVUI/NativeEventListener.h
    src/LVUI/Avatar.h
    src/LVUI/NativeAvatar.h
    src/LVUI/Tooltip.h
    src/LVUI/TooltipOutline.h
    src/LVUI/TooltipController.h
    src/LVUI/NativeTooltipPresentation.h
    src/LVUI/NativeTooltipScheduler.h
    src/LVUI/NativeTooltipTarget.h
    src/LVUI/NativeTooltip.h
    src/LVUI/Slider.h
    src/LVUI/NativeSlider.h
    src/LVUI/Stepper.h
    src/LVUI/NativeStepper.h
    src/LVUI/ComboBox.h
    src/LVUI/NativeComboBox.h
    src/LVUI/SegmentedControl.h
    src/LVUI/NativeSegmentedControl.h
    src/LVUI/NativeStylePort.h
    src/LVUI/Route.h
    src/LVUI/Link.h
    src/LVUI/NativeLink.h
    src/LVUI/NativeImage.h
    src/LVUI/NativeDocument.h
    src/LVUI/Text.h
    src/LVUI/TextInput.h
    src/LVUI/InputField.h
    src/LVUI/NativeTextBoundary.h
    src/LVUI/NativeTextLayout.h
    src/LVUI/NativeInputField.h
    src/LVUI/ProgressBar.h
    src/LVUI/StyleTokens.h
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
    option(LVUI_TEST_LEGACY_LVRS "Enable historical tests that read the separate LVRS checkout" OFF)
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
    target_include_directories(LVUIMotionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME LVUI.Motion.Model COMMAND LVUIMotionTests)
    add_executable(LVUIOwnedProgressTests tests/progress_bar_owned.cpp)
    target_compile_features(LVUIOwnedProgressTests PRIVATE cxx_std_23)
    target_include_directories(LVUIOwnedProgressTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME LVUI.OwnedProgress.Model COMMAND LVUIOwnedProgressTests)
    add_executable(LVUIOwnedProgressCodegenTests tests/progress_bar_codegen.cpp)
    target_link_libraries(LVUIOwnedProgressCodegenTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.OwnedProgress.Codegen COMMAND LVUIOwnedProgressCodegenTests)
    add_executable(LVUINativeCoreValueTests tests/native_core_values.cpp)
    target_link_libraries(LVUINativeCoreValueTests PRIVATE LVUI::Native)
    foreach(_case text numeric keys binding containers dynamic_keys temporary nested_binding named named_temporary named_cpp named_binding owned_reads)
        add_test(NAME LVUI.NativeCoreValues.${_case} COMMAND LVUINativeCoreValueTests ${_case})
    endforeach()
    add_executable(LVUIProgressTests tests/progress.cpp)
    add_executable(LVUIHierarchyTests tests/hierarchy.cpp)
    add_executable(LVUIOpacityTests tests/opacity.cpp)
    target_link_libraries(LVUIOpacityTests PRIVATE LVUI::LVUI)
    foreach(_case values reactive shared fixed components framework delegate diagnostics)
        add_test(NAME LVUI.Opacity.${_case} COMMAND LVUIOpacityTests ${_case})
        set_tests_properties(LVUI.Opacity.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    target_link_libraries(LVUIHierarchyTests PRIVATE LVUI::LVUI)
    foreach(_case item list reactive controller diagnostics framework slots slot_context slot_forwarding slot_diagnostics delegate delegate_context delegate_forwarding delegate_diagnostics)
        add_test(NAME LVUI.Hierarchy.${_case} COMMAND LVUIHierarchyTests ${_case})
        set_tests_properties(LVUI.Hierarchy.${_case} PROPERTIES TIMEOUT 15)
    endforeach()
    target_link_libraries(LVUIProgressTests PRIVATE LVUI::LVUI)
    foreach(_case defaults inputs reactive scoped diagnostics)
        add_test(NAME LVUI.Progress.${_case} COMMAND LVUIProgressTests ${_case})
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
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/iiGUI/NativeWidgets.h" AND
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
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedProgressCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedImage SOURCE tests/fixtures/NativeOwnedImage.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_generated)
        lvui_generate(NAME NativeOwnedImageAdvanced SOURCE tests/fixtures/NativeOwnedImageAdvanced.lvui
            INCLUDES NativeOwnedImageFixture.h OUTPUT_VARIABLE _owned_image_advanced)
        lvui_generate(NAME NativeOwnedImageStatic SOURCE tests/fixtures/NativeOwnedImageStatic.lvui
            OUTPUT_VARIABLE _owned_image_static)
        add_library(LVUIOwnedImageCompile OBJECT tests/native_owned_image_compile.cpp "${_owned_image_generated}" "${_owned_image_advanced}" "${_owned_image_static}")
        target_include_directories(LVUIOwnedImageCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedImageCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedButton SOURCE tests/fixtures/NativeOwnedButton.lvui
            INCLUDES NativeOwnedButtonFixture.h OUTPUT_VARIABLE _owned_button_generated)
        add_library(LVUIOwnedButtonCompile OBJECT tests/native_owned_button_compile.cpp "${_owned_button_generated}")
        target_include_directories(LVUIOwnedButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedDivider SOURCE tests/fixtures/NativeOwnedDivider.lvui
            INCLUDES NativeOwnedDividerFixture.h OUTPUT_VARIABLE _owned_divider_generated)
        add_library(LVUIOwnedDividerCompile OBJECT tests/native_owned_divider_compile.cpp "${_owned_divider_generated}")
        target_include_directories(LVUIOwnedDividerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedDividerCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedToggleSwitch SOURCE tests/fixtures/NativeOwnedToggleSwitch.lvui
            INCLUDES NativeOwnedToggleSwitchFixture.h OUTPUT_VARIABLE _owned_toggle_generated)
        add_library(LVUIOwnedToggleSwitchCompile OBJECT tests/native_owned_toggle_switch_compile.cpp "${_owned_toggle_generated}")
        target_include_directories(LVUIOwnedToggleSwitchCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedToggleSwitchCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedCheckBox SOURCE tests/fixtures/NativeOwnedCheckBox.lvui
            INCLUDES NativeOwnedCheckBoxFixture.h OUTPUT_VARIABLE _owned_checkbox_generated)
        add_library(LVUIOwnedCheckBoxCompile OBJECT tests/native_owned_check_box_compile.cpp "${_owned_checkbox_generated}")
        target_include_directories(LVUIOwnedCheckBoxCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedCheckBoxCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedRadioButton SOURCE tests/fixtures/NativeOwnedRadioButton.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_generated)
        lvui_generate(NAME NativeOwnedRadioRows SOURCE tests/fixtures/NativeOwnedRadioRows.lvui
            INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE _owned_radio_rows_generated)
        add_library(LVUIOwnedRadioButtonCompile OBJECT tests/native_owned_radio_button_compile.cpp "${_owned_radio_generated}" "${_owned_radio_rows_generated}")
        target_include_directories(LVUIOwnedRadioButtonCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedRadioButtonCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedMaterial SOURCE tests/fixtures/NativeOwnedMaterial.lvui
            INCLUDES NativeOwnedMaterialFixture.h OUTPUT_VARIABLE _owned_material_generated)
        add_library(LVUIOwnedMaterialCompile OBJECT tests/native_owned_material_compile.cpp "${_owned_material_generated}")
        target_include_directories(LVUIOwnedMaterialCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedMaterialCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedLink SOURCE tests/fixtures/NativeOwnedLink.lvui
            INCLUDES NativeOwnedLinkFixture.h OUTPUT_VARIABLE _owned_link_generated)
        lvui_generate(NAME NativeOwnedLinkMethods SOURCE tests/fixtures/NativeOwnedLinkMethods.lvui
            INCLUDES NativeOwnedLinkMethodsFixture.h OUTPUT_VARIABLE _owned_link_methods_generated)
        add_library(LVUIOwnedLinkCompile OBJECT tests/native_owned_link_compile.cpp "${_owned_link_generated}" "${_owned_link_methods_generated}")
        target_include_directories(LVUIOwnedLinkCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedLinkCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedPageRouter SOURCE tests/fixtures/NativeOwnedPageRouter.lvui
            INCLUDES NativeOwnedPageRouterFixture.h OUTPUT_VARIABLE _owned_page_router_generated)
        add_library(LVUIOwnedPageRouterCompile OBJECT tests/native_owned_page_router_compile.cpp "${_owned_page_router_generated}")
        target_include_directories(LVUIOwnedPageRouterCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedPageRouterCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppCard SOURCE tests/fixtures/NativeOwnedAppCard.lvui
            INCLUDES NativeOwnedAppCardFixture.h OUTPUT_VARIABLE _owned_app_card_generated)
        add_library(LVUIOwnedAppCardCompile OBJECT tests/native_owned_app_card_compile.cpp "${_owned_app_card_generated}")
        target_include_directories(LVUIOwnedAppCardCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAppCardCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAppHeader SOURCE tests/fixtures/NativeOwnedAppHeader.lvui
            INCLUDES NativeOwnedAppHeaderFixture.h OUTPUT_VARIABLE _owned_app_header_generated)
        add_library(LVUIOwnedAppHeaderCompile OBJECT tests/native_owned_app_header_compile.cpp "${_owned_app_header_generated}")
        target_include_directories(LVUIOwnedAppHeaderCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAppHeaderCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedEventListener SOURCE tests/fixtures/NativeOwnedEventListener.lvui
            INCLUDES NativeOwnedEventListenerFixture.h OUTPUT_VARIABLE _owned_event_listener_generated)
        add_library(LVUIOwnedEventListenerCompile OBJECT tests/native_owned_event_listener_compile.cpp "${_owned_event_listener_generated}")
        target_include_directories(LVUIOwnedEventListenerCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedEventListenerCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeOwnedAvatar SOURCE tests/fixtures/NativeOwnedAvatar.lvui
            INCLUDES NativeOwnedAvatarFixture.h OUTPUT_VARIABLE _owned_avatar_generated)
        add_library(LVUIOwnedAvatarCompile OBJECT tests/native_owned_avatar_compile.cpp "${_owned_avatar_generated}")
        target_include_directories(LVUIOwnedAvatarCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" tests/fixtures
            "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUIOwnedAvatarCompile PRIVATE LVUI::Native)
        lvui_generate(NAME NativeReactive SOURCE tests/fixtures/NativeReactive.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_generated)
        add_executable(LVUINativeGeneratedTests tests/native_generated.cpp "${_native_generated}")
        target_include_directories(LVUINativeGeneratedTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
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
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeNestedTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeNested COMMAND LVUINativeNestedTests)
        set_tests_properties(LVUI.NativeNested PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeViewValues SOURCE tests/fixtures/NativeViewValues.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_view_values)
        lvui_generate(NAME NativeConditional SOURCE tests/fixtures/NativeConditional.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_conditional)
        add_executable(LVUINativeViewValueTests tests/native_view_values.cpp "${_native_view_values}" "${_native_conditional}")
        target_include_directories(LVUINativeViewValueTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeViewValueTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeViewValues COMMAND LVUINativeViewValueTests)
        set_tests_properties(LVUI.NativeViewValues PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeComponents SOURCE tests/fixtures/NativeComponents.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_components)
        lvui_generate(NAME NativeComponentRetirement SOURCE tests/fixtures/NativeComponentRetirement.lvui
            INCLUDES NativeFixture.h OUTPUT_VARIABLE _native_component_retirement)
        add_executable(LVUINativeComponentTests tests/native_components.cpp "${_native_components}" "${_native_component_retirement}")
        target_include_directories(LVUINativeComponentTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeComponentTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeComponents COMMAND LVUINativeComponentTests)
        set_tests_properties(LVUI.NativeComponents PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeScalarInputs SOURCE tests/fixtures/NativeScalarInputs.lvui
            INCLUDES NativeScalarInputFixture.h OUTPUT_VARIABLE _native_scalar_inputs)
        add_executable(LVUINativeScalarInputTests tests/native_scalar_inputs.cpp "${_native_scalar_inputs}")
        target_include_directories(LVUINativeScalarInputTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeScalarInputTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeScalarInputs COMMAND LVUINativeScalarInputTests)
        set_tests_properties(LVUI.NativeScalarInputs PROPERTIES TIMEOUT 15)
        lvui_generate(NAME NativeLists SOURCE tests/fixtures/NativeLists.lvui
            INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _native_lists)
        add_executable(LVUINativeListTests tests/native_lists.cpp "${_native_lists}")
        target_include_directories(LVUINativeListTests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
            tests/fixtures "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
        target_link_libraries(LVUINativeListTests PRIVATE LVUI::Native)
        add_test(NAME LVUI.NativeLists COMMAND LVUINativeListTests)
        set_tests_properties(LVUI.NativeLists PROPERTIES TIMEOUT 15)
        set(LVUI_LVRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../LVRS" CACHE PATH "Optional LVRS headers for compile-only repetition coverage")
        if(LVUI_TEST_LEGACY_LVRS AND EXISTS "${LVUI_LVRS_SOURCE_DIR}/src/LVRS/Widgets.h")
            lvui_generate(NAME LVRSRepetition SOURCE tests/fixtures/NativeRepetition.lvui FRAMEWORK LVRS
                INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _lvrs_repetition)
            lvui_generate(NAME LVRSSharedRepetition SOURCE tests/fixtures/NativeSharedRepetition.lvui FRAMEWORK LVRS
                INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _lvrs_shared_repetition)
            lvui_generate(NAME LVRSVirtualRepetition SOURCE tests/fixtures/NativeVirtualRepetition.lvui FRAMEWORK LVRS
                INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _lvrs_virtual_repetition)
            lvui_generate(NAME LVRSMeasuredRepetition SOURCE tests/fixtures/NativeMeasuredRepetition.lvui FRAMEWORK LVRS
                INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _lvrs_measured_repetition)
            lvui_generate(NAME LVRSNavigatedRepetition SOURCE tests/fixtures/NativeNavigatedRepetition.lvui FRAMEWORK LVRS
                INCLUDES NativeRepetitionFixture.h OUTPUT_VARIABLE _lvrs_navigated_repetition)
            lvui_generate(NAME LVRSReturnedCallbacks SOURCE tests/fixtures/NativeReturnedCallbacks.lvui FRAMEWORK LVRS
                INCLUDES NativeReturnedFixture.h OUTPUT_VARIABLE _lvrs_returned_callbacks)
            lvui_generate(NAME LVRSBackendConstruction SOURCE tests/fixtures/NativeBackendConstruction.lvui FRAMEWORK LVRS
                INCLUDES NativeConstructionFixture.h OUTPUT_VARIABLE _lvrs_backend_construction)
            lvui_generate(NAME LVRSCallableInputs SOURCE tests/fixtures/NativeCallableInputs.lvui FRAMEWORK LVRS
                INCLUDES NativeCallableFixture.h OUTPUT_VARIABLE _lvrs_callable_inputs)
            lvui_generate(NAME LVRSCallableEvents SOURCE tests/fixtures/NativeCallableEvents.lvui FRAMEWORK LVRS
                INCLUDES NativeCallableEventFixture.h OUTPUT_VARIABLE _lvrs_callable_events)
            lvui_generate(NAME LVRSSharedCallable SOURCE tests/fixtures/NativeSharedCallable.lvui FRAMEWORK LVRS
                INCLUDES NativeSharedCallableFixture.h OUTPUT_VARIABLE _lvrs_shared_callable)
            lvui_generate(NAME LVRSIndependentCallable SOURCE tests/fixtures/NativeIndependentCallable.lvui FRAMEWORK LVRS
                INCLUDES NativeSharedCallableFixture.h OUTPUT_VARIABLE _lvrs_independent_callable)
            lvui_generate(NAME LVRSRootSharedCallable SOURCE tests/fixtures/NativeRootSharedCallable.lvui FRAMEWORK LVRS
                INCLUDES NativeSharedCallableFixture.h OUTPUT_VARIABLE _lvrs_root_shared_callable)
            lvui_generate(NAME LVRSReferenceCallable SOURCE tests/fixtures/NativeReferenceCallable.lvui FRAMEWORK LVRS
                INCLUDES NativeSharedCallableFixture.h OUTPUT_VARIABLE _lvrs_reference_callable)
            lvui_generate(NAME LVRSFunctionValues SOURCE tests/fixtures/NativeFunctionValues.lvui FRAMEWORK LVRS
                INCLUDES NativeFunctionFixture.h OUTPUT_VARIABLE _lvrs_function_values)
            lvui_generate(NAME LVRSFunctionValidators SOURCE tests/fixtures/NativeFunctionValidators.lvui FRAMEWORK LVRS
                INCLUDES NativeFunctionValidatorFixture.h OUTPUT_VARIABLE _lvrs_function_validators)
            lvui_generate(NAME LVRSFunctionVariables SOURCE tests/fixtures/NativeFunctionVariables.lvui FRAMEWORK LVRS
                INCLUDES NativeFunctionVariableFixture.h OUTPUT_VARIABLE _lvrs_function_variables)
            add_library(LVUILVRSRepetitionCompile OBJECT tests/lvrs_repetition_compile.cpp tests/lvrs_callable_compile.cpp "${_lvrs_repetition}" "${_lvrs_shared_repetition}" "${_lvrs_virtual_repetition}" "${_lvrs_measured_repetition}" "${_lvrs_navigated_repetition}" "${_lvrs_returned_callbacks}" "${_lvrs_backend_construction}" "${_lvrs_callable_inputs}" "${_lvrs_callable_events}")
            target_sources(LVUILVRSRepetitionCompile PRIVATE "${_lvrs_shared_callable}" "${_lvrs_independent_callable}" "${_lvrs_root_shared_callable}" "${_lvrs_reference_callable}")
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_function_compile.cpp "${_lvrs_function_values}" "${_lvrs_function_validators}")
            target_sources(LVUILVRSRepetitionCompile PRIVATE "${_lvrs_function_variables}")
            set(_lvrs_view_sequences)
            foreach(_sequence ViewSequences ViewSequenceRows ViewSequenceRoot ViewSequenceOverride)
                lvui_generate(NAME LVRS${_sequence} SOURCE tests/fixtures/Native${_sequence}.lvui FRAMEWORK LVRS
                    INCLUDES NativeViewSequenceFixture.h OUTPUT_VARIABLE _lvrs_view_sequence)
                list(APPEND _lvrs_view_sequences "${_lvrs_view_sequence}")
            endforeach()
            lvui_generate(NAME LVRSViewSequenceControls SOURCE tests/fixtures/LVRSViewSequenceControls.lvui FRAMEWORK LVRS
                OUTPUT_VARIABLE _lvrs_sequence_controls)
            list(APPEND _lvrs_view_sequences "${_lvrs_sequence_controls}")
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_view_sequences_compile.cpp ${_lvrs_view_sequences})
            lvui_generate(NAME LVRSNativeColors SOURCE tests/fixtures/LVRSNativeColors.lvui FRAMEWORK LVRS
                INCLUDES NativeColorFixture.h OUTPUT_VARIABLE _lvrs_native_colors)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_colors_compile.cpp "${_lvrs_native_colors}")
            set(_lvrs_scalar_inputs)
            foreach(_scalar ScalarInputs ScalarForward ScalarTypes ScalarConditional ScalarRoot ScalarFunctionDefault)
                lvui_generate(NAME LVRS${_scalar} SOURCE tests/fixtures/Native${_scalar}.lvui FRAMEWORK LVRS
                    INCLUDES NativeScalarInputFixture.h OUTPUT_VARIABLE _lvrs_scalar_input)
                list(APPEND _lvrs_scalar_inputs "${_lvrs_scalar_input}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_scalar_inputs_compile.cpp ${_lvrs_scalar_inputs})
            set(_lvrs_layouts)
            foreach(_layout Layout LayoutAlignment LayoutConditional LayoutMeasured)
                lvui_generate(NAME LVRS${_layout} SOURCE tests/fixtures/Native${_layout}.lvui FRAMEWORK LVRS
                    INCLUDES NativeLayoutFixture.h OUTPUT_VARIABLE _lvrs_layout)
                list(APPEND _lvrs_layouts "${_lvrs_layout}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_layout_compile.cpp ${_lvrs_layouts})
            lvui_generate(NAME LVRSLayoutControls SOURCE tests/fixtures/LVRSLayoutControls.lvui FRAMEWORK LVRS
                INCLUDES NativeLayoutFixture.h OUTPUT_VARIABLE _lvrs_layout_controls)
            target_sources(LVUILVRSRepetitionCompile PRIVATE "${_lvrs_layout_controls}")
            set(_lvrs_windows)
            foreach(_window Window WindowRoot WindowStatic)
                lvui_generate(NAME LVRS${_window} SOURCE tests/fixtures/Native${_window}.lvui FRAMEWORK LVRS
                    INCLUDES NativeWindowFixture.h OUTPUT_VARIABLE _lvrs_window)
                list(APPEND _lvrs_windows "${_lvrs_window}")
            endforeach()
            foreach(_window WindowAccent WindowAccentName)
                lvui_generate(NAME LVRS${_window} SOURCE tests/fixtures/LVRS${_window}.lvui FRAMEWORK LVRS
                    INCLUDES NativeWindowAccentFixture.h OUTPUT_VARIABLE _lvrs_window)
                list(APPEND _lvrs_windows "${_lvrs_window}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_window_compile.cpp ${_lvrs_windows})
            set(_lvrs_readonly_views)
            foreach(_readonly Readonly ReadonlyPostfix)
                lvui_generate(NAME LVRS${_readonly} SOURCE tests/fixtures/Native${_readonly}.lvui FRAMEWORK LVRS
                    INCLUDES NativeReadonlyFixture.h OUTPUT_VARIABLE _lvrs_readonly)
                list(APPEND _lvrs_readonly_views "${_lvrs_readonly}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_readonly_compile.cpp ${_lvrs_readonly_views})
            set(_lvrs_object_inputs)
            foreach(_object ObjectInputs ObjectHandles ObjectRoot ObjectRows)
                lvui_generate(NAME LVRS${_object} SOURCE tests/fixtures/Native${_object}.lvui FRAMEWORK LVRS
                    INCLUDES NativeObjectInputFixture.h OUTPUT_VARIABLE _lvrs_object_input)
                list(APPEND _lvrs_object_inputs "${_lvrs_object_input}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_object_inputs_compile.cpp ${_lvrs_object_inputs})
            set(_lvrs_method_queries)
            foreach(_method MethodQueries MethodPostfix)
                lvui_generate(NAME LVRS${_method} SOURCE tests/fixtures/Native${_method}.lvui FRAMEWORK LVRS
                    INCLUDES NativeMethodQueryFixture.h OUTPUT_VARIABLE _lvrs_method_query)
                list(APPEND _lvrs_method_queries "${_lvrs_method_query}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_method_queries_compile.cpp ${_lvrs_method_queries})
            set(_lvrs_object_methods)
            foreach(_method ObjectMethodReferences ObjectMethodPostfix)
                lvui_generate(NAME LVRS${_method} SOURCE tests/fixtures/Native${_method}.lvui FRAMEWORK LVRS
                    INCLUDES NativeObjectMethodReferenceFixture.h OUTPUT_VARIABLE _lvrs_object_method)
                list(APPEND _lvrs_object_methods "${_lvrs_object_method}")
            endforeach()
            lvui_generate(NAME LVRSObjectMethodButtons SOURCE tests/fixtures/LVRSObjectMethodReferences.lvui FRAMEWORK LVRS
                INCLUDES NativeObjectMethodReferenceFixture.h OUTPUT_VARIABLE _lvrs_object_buttons)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_object_method_references_compile.cpp ${_lvrs_object_methods} "${_lvrs_object_buttons}")
            set(_lvrs_callable_fields)
            foreach(_view CallableFields CallableFieldPostfix)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeCallableFieldFixture.h OUTPUT_VARIABLE _lvrs_callable_field)
                list(APPEND _lvrs_callable_fields "${_lvrs_callable_field}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_callable_fields_compile.cpp ${_lvrs_callable_fields})
            set(_lvrs_factory_captures)
            foreach(_view FactoryCaptures FactoryCapturePostfix)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeFactoryCaptureFixture.h OUTPUT_VARIABLE _lvrs_factory_capture)
                list(APPEND _lvrs_factory_captures "${_lvrs_factory_capture}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_factory_captures_compile.cpp ${_lvrs_factory_captures})
            lvui_generate(NAME LVRSInferredValues SOURCE tests/fixtures/NativeInferredValues.lvui FRAMEWORK LVRS
                INCLUDES NativeFactoryCaptureFixture.h OUTPUT_VARIABLE _lvrs_inferred_values)
            lvui_generate(NAME LVRSInferredFields SOURCE tests/fixtures/NativeInferredFields.lvui FRAMEWORK LVRS
                INCLUDES NativeInferredFieldsFixture.h OUTPUT_VARIABLE _lvrs_inferred_fields)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_inferred_values_compile.cpp
                "${_lvrs_inferred_values}" "${_lvrs_inferred_fields}")
            lvui_generate(NAME LVRSPresentation SOURCE tests/fixtures/NativePresentation.lvui FRAMEWORK LVRS
                INCLUDES NativePresentationFixture.h OUTPUT_VARIABLE _lvrs_presentation)
            lvui_generate(NAME LVRSScopedPresentation SOURCE tests/fixtures/NativeScopedPresentation.lvui FRAMEWORK LVRS
                INCLUDES NativePresentationFixture.h OUTPUT_VARIABLE _lvrs_scoped_presentation)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_presentation_compile.cpp
                "${_lvrs_presentation}" "${_lvrs_scoped_presentation}")
            set(_lvrs_progress_views)
            foreach(_view Progress ProgressScoped ProgressForwarded)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeProgressFixture.h OUTPUT_VARIABLE _lvrs_progress)
                list(APPEND _lvrs_progress_views "${_lvrs_progress}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_progress_compile.cpp ${_lvrs_progress_views})
            set(_lvrs_history_views)
            foreach(_view History HistoryScoped)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeHistoryFixture.h OUTPUT_VARIABLE _lvrs_history)
                list(APPEND _lvrs_history_views "${_lvrs_history}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_history_compile.cpp ${_lvrs_history_views})
            set(_lvrs_registry_views)
            foreach(_view Registry RegistryScoped)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeRegistryFixture.h OUTPUT_VARIABLE _lvrs_registry)
                list(APPEND _lvrs_registry_views "${_lvrs_registry}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_registry_compile.cpp ${_lvrs_registry_views})
            lvui_generate(NAME LVRSRouteRegistry SOURCE tests/fixtures/NativeRouteRegistry.lvui FRAMEWORK LVRS
                INCLUDES NativeRouteRegistryFixture.h OUTPUT_VARIABLE _lvrs_route_registry)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_route_registry_compile.cpp "${_lvrs_route_registry}")
            set(_lvrs_hierarchy_views)
            foreach(_view HierarchyList HierarchyItem HierarchyCount HierarchyDelegate)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeHierarchyFixture.h OUTPUT_VARIABLE _lvrs_hierarchy)
                list(APPEND _lvrs_hierarchy_views "${_lvrs_hierarchy}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_hierarchy_compile.cpp ${_lvrs_hierarchy_views})
            lvui_generate(NAME LVRSOpacity SOURCE tests/fixtures/NativeOpacity.lvui FRAMEWORK LVRS
                INCLUDES NativeOpacityFixture.h OUTPUT_VARIABLE _lvrs_opacity)
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_opacity_compile.cpp "${_lvrs_opacity}")
            set(_lvrs_platform_views)
            foreach(_view Platform PlatformSelection)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativePlatformFixture.h OUTPUT_VARIABLE _lvrs_platform)
                list(APPEND _lvrs_platform_views "${_lvrs_platform}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_platform_compile.cpp ${_lvrs_platform_views})
            set(_lvrs_model_source_views)
            foreach(_view ModelSource ModelSourceSelection)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeModelSourceFixture.h OUTPUT_VARIABLE _lvrs_model_source)
                list(APPEND _lvrs_model_source_views "${_lvrs_model_source}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_model_source_compile.cpp ${_lvrs_model_source_views})
            set(_lvrs_font_views)
            foreach(_view Font FontSelection)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeFontFixture.h OUTPUT_VARIABLE _lvrs_font)
                list(APPEND _lvrs_font_views "${_lvrs_font}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_font_compile.cpp ${_lvrs_font_views})
            set(_lvrs_svg_views)
            foreach(_view Svg SvgSelection)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeSvgFixture.h OUTPUT_VARIABLE _lvrs_svg)
                list(APPEND _lvrs_svg_views "${_lvrs_svg}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_svg_compile.cpp ${_lvrs_svg_views})
            set(_lvrs_render_views)
            foreach(_view Render RenderSelection)
                lvui_generate(NAME LVRS${_view} SOURCE tests/fixtures/Native${_view}.lvui FRAMEWORK LVRS
                    INCLUDES NativeRenderFixture.h OUTPUT_VARIABLE _lvrs_render)
                list(APPEND _lvrs_render_views "${_lvrs_render}")
            endforeach()
            target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_render_compile.cpp ${_lvrs_render_views})
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../iiSDKCore/tests/fixtures/observable_module/Model.h")
                lvui_generate(NAME LVRSSharedMethod SOURCE tests/fixtures/NativeSharedMethod.lvui FRAMEWORK LVRS
                    INCLUDES NativeSharedMethodFixture.h OUTPUT_VARIABLE _lvrs_shared_method)
                target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_shared_method_compile.cpp "${_lvrs_shared_method}")
                target_include_directories(LVUILVRSRepetitionCompile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../iiSDKCore/tests")
            endif()
            if(LVUI_BUILD_RUNTIME)
                lvui_generate(NAME LVRSActivationWork SOURCE tests/fixtures/NativeWork.lvui FRAMEWORK LVRS
                    INCLUDES NativeWorkFixture.h OUTPUT_VARIABLE _lvrs_activation_work)
                target_sources(LVUILVRSRepetitionCompile PRIVATE tests/lvrs_work_compile.cpp "${_lvrs_activation_work}")
            endif()
            target_include_directories(LVUILVRSRepetitionCompile PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
                tests/fixtures "${LVUI_LVRS_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/src" "${LVUI_IIGUI_SOURCE_DIR}/build/install/include")
            target_link_libraries(LVUILVRSRepetitionCompile PRIVATE LVUI::Native)
            set_target_properties(LVUILVRSRepetitionCompile PROPERTIES CXX_EXTENSIONS OFF)
        endif()
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
    add_executable(LVUICompilerTests tests/compiler.cpp)
    target_link_libraries(LVUICompilerTests PRIVATE LVUI::LVUI)
    add_test(NAME LVUI.AOT.lvrs_code_editor COMMAND LVUICompilerTests lvrs_code_editor)
    add_test(NAME LVUI.AOT.lvrs_tooltip COMMAND LVUICompilerTests lvrs_tooltip)
    foreach(_case generation validation actions escaping native_widget embedded_build lvrs_router lvrs_link lvrs_app_surfaces lvrs_materials lvrs_media lvrs_toolbars lvrs_controls lvrs_compound lvrs_menus lvrs_color_fields lvrs_color_picker lvrs_popover lvrs_color_button lvrs_modal lvrs_alerts lvrs_sheet lvrs_tables lvrs_table_view markup lvrs_lists editor_styles)
        add_test(NAME LVUI.AOT.${_case} COMMAND LVUICompilerTests ${_case})
        set_tests_properties(LVUI.AOT.${_case} PROPERTIES TIMEOUT 10)
    endforeach()
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    if(EXISTS "${LVUI_IIGUI_SOURCE_DIR}/src/iiGUI/NativeWidgets.h" AND
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
        if(LVUI_TEST_LEGACY_LVRS)
            add_test(NAME LVUI.NativeWindowValidation COMMAND "${Python3_EXECUTABLE}" -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_window_validation.py"
                --build "${CMAKE_CURRENT_BINARY_DIR}" --compiler "$<TARGET_FILE:lvui-compile>"
                --cxx "${CMAKE_CXX_COMPILER}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}"
                --gui "${LVUI_IIGUI_SOURCE_DIR}" --lvrs "${LVUI_LVRS_SOURCE_DIR}")
            set_tests_properties(LVUI.NativeWindowValidation PROPERTIES TIMEOUT 120)
        endif()
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


    endif()
    add_test(NAME LVUI.NativeInstalled COMMAND "${Python3_EXECUTABLE}" -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_install.py"
        --build "${CMAKE_CURRENT_BINARY_DIR}" --core "${iiSDKCore_DIR}" --color "${iiColorManager_DIR}" --cmake "${CMAKE_COMMAND}")
    # This check builds three fresh relocated consumers and negative C++ probes.
    # Its cold compiler/package time is separate from UI or runtime latency.
    # Allow the complete package build under sanitizers and cold OS executable assessment.
    set_tests_properties(LVUI.NativeInstalled PROPERTIES TIMEOUT 360)
    if(LVUI_TEST_LEGACY_LVRS)
        add_test(NAME LVUI.QmlObservation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_qml_observation.py")
        set_tests_properties(LVUI.QmlObservation PROPERTIES TIMEOUT 30)
    endif()
    if(LVUI_TEST_LEGACY_LVRS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../LVRS/tools/native_platform_contract.py")
        add_test(NAME LVUI.PlatformContract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_platform_contract.py")
        set_tests_properties(LVUI.PlatformContract PROPERTIES TIMEOUT 30)
        add_test(NAME LVUI.ModelSourceContract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_model_source_contract.py")
        set_tests_properties(LVUI.ModelSourceContract PROPERTIES TIMEOUT 30)
        add_test(NAME LVUI.FontContract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_font_contract.py")
        set_tests_properties(LVUI.FontContract PROPERTIES TIMEOUT 30)
        add_test(NAME LVUI.SvgContract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_svg_contract.py")
        set_tests_properties(LVUI.SvgContract PROPERTIES TIMEOUT 30)
        add_test(NAME LVUI.RenderContract COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_render_contract.py")
        set_tests_properties(LVUI.RenderContract PROPERTIES TIMEOUT 30)
    endif()
    set(LVUI_DREAMSCAPES_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Product/Dreamscapes/src/App"
        CACHE PATH "Optional read-only Dreamscapes QML corpus for completion audit")
    if(LVUI_TEST_LEGACY_LVRS AND EXISTS "${LVUI_DREAMSCAPES_SOURCE_DIR}/Main.qml")
        add_test(NAME LVUI.DreamscapesObservation COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/observe_qml.py" --check
            --source "${LVUI_DREAMSCAPES_SOURCE_DIR}")
        set_tests_properties(LVUI.DreamscapesObservation PROPERTIES TIMEOUT 60)
    endif()
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
    if(LVUI_TEST_LEGACY_LVRS)
        add_test(NAME LVUI.LegacyKeywordAuditTests COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_keyword_inventory.py")
    endif()
    set(LVUI_LVRS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../LVRS" CACHE PATH "Optional LVRS source for keyword drift verification")
    if(LVUI_TEST_LEGACY_LVRS AND EXISTS "${LVUI_LVRS_SOURCE_DIR}/src/qml/CMakeLists.txt")
        add_test(NAME LVUI.LVRSInventory COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/update_lvrs_keywords.py" --lvrs-root "${LVUI_LVRS_SOURCE_DIR}" --check)
    endif()
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
