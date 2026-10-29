cmake_minimum_required(VERSION 3.31)
project(LVUIOwnedToggleConsumer LANGUAGES CXX)

option(LVUI_CONSUMER_TEST_COLOR "Exercise the optional LVUI color surface adapter" OFF)
set(_components Native)
if(LVUI_CONSUMER_TEST_COLOR)
    list(APPEND _components Color)
endif()
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS ${_components})
find_package(iiGUI CONFIG REQUIRED)
lvui_generate(NAME NativeOwnedToggleSwitch SOURCE ../fixtures/NativeOwnedToggleSwitch.lvui
    INCLUDES NativeOwnedToggleSwitchFixture.h OUTPUT_VARIABLE generated)
lvui_generate(NAME NativeOwnedCheckBox SOURCE ../fixtures/NativeOwnedCheckBox.lvui
    INCLUDES NativeOwnedCheckBoxFixture.h OUTPUT_VARIABLE generated_checkbox)
lvui_generate(NAME NativeOwnedRadioButton SOURCE ../fixtures/NativeOwnedRadioButton.lvui
    INCLUDES NativeOwnedRadioButtonFixture.h OUTPUT_VARIABLE generated_radio)
lvui_generate(NAME NativeOwnedMaterial SOURCE ../fixtures/NativeOwnedMaterial.lvui
    INCLUDES NativeOwnedMaterialFixture.h OUTPUT_VARIABLE generated_material)
lvui_generate(NAME NativeOwnedLink SOURCE ../fixtures/NativeOwnedLink.lvui
    INCLUDES NativeOwnedLinkFixture.h OUTPUT_VARIABLE generated_link)
lvui_generate(NAME NativeOwnedLinkMethods SOURCE ../fixtures/NativeOwnedLinkMethods.lvui
    INCLUDES NativeOwnedLinkMethodsFixture.h OUTPUT_VARIABLE generated_link_methods)
lvui_generate(NAME NativeOwnedPageRouter SOURCE ../fixtures/NativeOwnedPageRouter.lvui
    INCLUDES NativeOwnedPageRouterFixture.h OUTPUT_VARIABLE generated_page_router)
lvui_generate(NAME NativeOwnedAppCard SOURCE ../fixtures/NativeOwnedAppCard.lvui
    INCLUDES NativeOwnedAppCardFixture.h OUTPUT_VARIABLE generated_app_card)
lvui_generate(NAME NativeOwnedAppHeader SOURCE ../fixtures/NativeOwnedAppHeader.lvui
    INCLUDES NativeOwnedAppHeaderFixture.h OUTPUT_VARIABLE generated_app_header)
lvui_generate(NAME NativeOwnedEventListener SOURCE ../fixtures/NativeOwnedEventListener.lvui
    INCLUDES NativeOwnedEventListenerFixture.h OUTPUT_VARIABLE generated_event_listener)
lvui_generate(NAME NativeOwnedAvatar SOURCE ../fixtures/NativeOwnedAvatar.lvui
    INCLUDES NativeOwnedAvatarFixture.h OUTPUT_VARIABLE generated_avatar)
lvui_generate(NAME NativeOwnedTooltip SOURCE ../fixtures/NativeOwnedTooltip.lvui
    INCLUDES NativeOwnedTooltipFixture.h OUTPUT_VARIABLE generated_tooltip)
lvui_generate(NAME NativeOwnedTooltipInline SOURCE ../fixtures/NativeOwnedTooltipInline.lvui
    INCLUDES NativeOwnedTooltipFixture.h OUTPUT_VARIABLE generated_tooltip_inline)
lvui_generate(NAME NativeOwnedSlider SOURCE ../fixtures/NativeOwnedSlider.lvui
    INCLUDES NativeOwnedSliderFixture.h OUTPUT_VARIABLE generated_slider)
lvui_generate(NAME NativeOwnedInputField SOURCE ../fixtures/NativeOwnedInputField.lvui
    INCLUDES NativeOwnedInputFieldFixture.h OUTPUT_VARIABLE generated_input_field)
lvui_generate(NAME NativeOwnedStepper SOURCE ../fixtures/NativeOwnedStepper.lvui
    INCLUDES NativeOwnedStepperFixture.h OUTPUT_VARIABLE generated_stepper)
lvui_generate(NAME NativeOwnedComboBox SOURCE ../fixtures/NativeOwnedComboBox.lvui
    INCLUDES NativeOwnedComboBoxFixture.h OUTPUT_VARIABLE generated_combo_box)
lvui_generate(NAME NativeOwnedSegmentedControl SOURCE ../fixtures/NativeOwnedSegmentedControl.lvui
    INCLUDES NativeOwnedSegmentedControlFixture.h OUTPUT_VARIABLE generated_segmented_control)
lvui_generate(NAME NativeOwnedHelpButton SOURCE ../fixtures/NativeOwnedHelpButton.lvui
    INCLUDES NativeOwnedHelpButtonFixture.h OUTPUT_VARIABLE generated_help_button)
lvui_generate(NAME NativeOwnedDropdownButton SOURCE ../fixtures/NativeOwnedDropdownButton.lvui
    INCLUDES NativeOwnedDropdownButtonFixture.h OUTPUT_VARIABLE generated_dropdown_button)
lvui_generate(NAME NativeOwnedNamedIcons SOURCE ../fixtures/NativeOwnedNamedIcons.lvui
    INCLUDES NativeOwnedNamedIconsFixture.h OUTPUT_VARIABLE generated_named_icons)
lvui_generate(NAME NativeOwnedTab SOURCE ../fixtures/NativeOwnedTab.lvui
    INCLUDES NativeOwnedTabFixture.h OUTPUT_VARIABLE generated_tab)
lvui_generate(NAME NativeOwnedTabBar SOURCE ../fixtures/NativeOwnedTabBar.lvui
    INCLUDES NativeOwnedTabBarFixture.h OUTPUT_VARIABLE generated_tab_bar)
lvui_generate(NAME NativeOwnedMenuItem SOURCE ../fixtures/NativeOwnedMenuItem.lvui
    INCLUDES NativeOwnedMenuItemFixture.h OUTPUT_VARIABLE generated_menu_item)
lvui_generate(NAME NativeOwnedMenu SOURCE ../fixtures/NativeOwnedMenu.lvui
    INCLUDES NativeOwnedMenuFixture.h OUTPUT_VARIABLE generated_menu)
lvui_generate(NAME NativeOwnedSubmenu SOURCE ../fixtures/NativeOwnedSubmenu.lvui
    INCLUDES NativeOwnedSubmenuFixture.h OUTPUT_VARIABLE generated_submenu)
lvui_generate(NAME NativeOwnedPopover SOURCE ../fixtures/NativeOwnedPopover.lvui
    INCLUDES NativeOwnedPopoverFixture.h OUTPUT_VARIABLE generated_popover)
lvui_generate(NAME NativeOwnedModal SOURCE ../fixtures/NativeOwnedModal.lvui
    INCLUDES NativeOwnedModalFixture.h OUTPUT_VARIABLE generated_modal)
lvui_generate(NAME NativeOwnedAlertButton SOURCE ../fixtures/NativeOwnedAlertButton.lvui
    INCLUDES NativeOwnedAlertButtonFixture.h OUTPUT_VARIABLE generated_alert_button)
lvui_generate(NAME NativeOwnedAlert SOURCE ../fixtures/NativeOwnedAlert.lvui
    INCLUDES NativeOwnedAlertFixture.h OUTPUT_VARIABLE generated_alert)
lvui_generate(NAME NativeOwnedSheet SOURCE ../fixtures/NativeOwnedSheet.lvui
    INCLUDES NativeOwnedSheetFixture.h OUTPUT_VARIABLE generated_sheet)
lvui_generate(NAME NativeOwnedTable SOURCE ../fixtures/NativeOwnedTable.lvui
    INCLUDES NativeOwnedTableFixture.h OUTPUT_VARIABLE generated_table)
lvui_generate(NAME NativeOwnedCodeEditor SOURCE ../fixtures/NativeOwnedCodeEditor.lvui
    INCLUDES NativeOwnedCodeEditorFixture.h OUTPUT_VARIABLE generated_code_editor)
lvui_generate(NAME NativeOwnedListToolbar SOURCE ../fixtures/NativeOwnedListToolbar.lvui
    INCLUDES NativeOwnedListToolbarFixture.h OUTPUT_VARIABLE generated_list_toolbar)
lvui_generate(NAME NativeOwnedToolbarButton SOURCE ../fixtures/NativeOwnedToolbarButton.lvui
    INCLUDES NativeOwnedToolbarButtonFixture.h OUTPUT_VARIABLE generated_toolbar_button)
lvui_generate(NAME NativeOwnedListFooter SOURCE ../fixtures/NativeOwnedListFooter.lvui
    INCLUDES NativeOwnedListFooterFixture.h OUTPUT_VARIABLE generated_list_footer)
lvui_generate(NAME NativeOwnedHierarchyToolbar SOURCE ../fixtures/NativeOwnedHierarchyToolbar.lvui
    INCLUDES NativeOwnedHierarchyToolbarFixture.h OUTPUT_VARIABLE generated_hierarchy_toolbar)
lvui_generate(NAME NativeOwnedHierarchyPanel SOURCE ../fixtures/NativeOwnedHierarchyPanel.lvui
    INCLUDES NativeOwnedHierarchyPanelFixture.h OUTPUT_VARIABLE generated_hierarchy_panel)
lvui_generate(NAME NativeOwnedListPanel SOURCE ../fixtures/NativeOwnedListPanel.lvui
    INCLUDES NativeOwnedListPanelFixture.h OUTPUT_VARIABLE generated_list_panel)
lvui_generate(NAME NativeOwnedListItem SOURCE ../fixtures/NativeOwnedListItem.lvui
    INCLUDES NativeOwnedListItemFixture.h OUTPUT_VARIABLE generated_list_item)
lvui_generate(NAME NativeOwnedListItemComposite SOURCE ../fixtures/NativeOwnedListItemComposite.lvui
    INCLUDES NativeOwnedListItemCompositeFixture.h OUTPUT_VARIABLE generated_list_item_composite)
lvui_generate(NAME NativeOwnedListItemActions SOURCE ../fixtures/NativeOwnedListItemActions.lvui
    INCLUDES NativeOwnedListItemActionsFixture.h OUTPUT_VARIABLE generated_list_item_actions)
lvui_generate(NAME NativeOwnedListItemStepper SOURCE ../fixtures/NativeOwnedListItemStepper.lvui
    INCLUDES NativeOwnedListItemStepperFixture.h OUTPUT_VARIABLE generated_list_item_stepper)
lvui_generate(NAME NativeOwnedListItemSelect SOURCE ../fixtures/NativeOwnedListItemSelect.lvui
    INCLUDES NativeOwnedListItemSelectFixture.h OUTPUT_VARIABLE generated_list_item_select)
lvui_generate(NAME NativeOwnedCard SOURCE ../fixtures/NativeOwnedCard.lvui
    INCLUDES NativeOwnedCardFixture.h OUTPUT_VARIABLE generated_card)
lvui_generate(NAME NativeOwnedTableHeader SOURCE ../fixtures/NativeOwnedTableHeader.lvui
    INCLUDES NativeOwnedTableHeaderFixture.h OUTPUT_VARIABLE generated_table_header)
lvui_generate(NAME NativeOwnedTableCellItem SOURCE ../fixtures/NativeOwnedTableCellItem.lvui
    INCLUDES NativeOwnedTableCellItemFixture.h OUTPUT_VARIABLE generated_table_cell_item)
lvui_generate(NAME NativeOwnedTableRow SOURCE ../fixtures/NativeOwnedTableRow.lvui
    INCLUDES NativeOwnedTableRowFixture.h OUTPUT_VARIABLE generated_table_row)
lvui_generate(NAME NativeOwnedHierarchyList SOURCE ../fixtures/NativeOwnedHierarchyList.lvui
    INCLUDES NativeOwnedHierarchyListFixture.h OUTPUT_VARIABLE generated_hierarchy_list)
lvui_generate(NAME NativeOwnedHierarchyItem SOURCE ../fixtures/NativeOwnedHierarchyItem.lvui
    INCLUDES NativeOwnedHierarchyItemFixture.h OUTPUT_VARIABLE generated_hierarchy_item)
lvui_generate(NAME NativeOwnedColorButton SOURCE ../fixtures/NativeOwnedColorButton.lvui
    INCLUDES NativeOwnedColorButtonFixture.h OUTPUT_VARIABLE generated_color_button)
add_executable(toggle_consumer main.cpp "${generated}" "${generated_checkbox}" "${generated_radio}" "${generated_material}" "${generated_link}" "${generated_link_methods}" "${generated_page_router}" "${generated_app_card}" "${generated_app_header}" "${generated_event_listener}" "${generated_avatar}" "${generated_tooltip}" "${generated_tooltip_inline}" "${generated_slider}" "${generated_input_field}" "${generated_stepper}" "${generated_combo_box}" "${generated_segmented_control}" "${generated_help_button}" "${generated_dropdown_button}")
target_compile_features(toggle_consumer PRIVATE cxx_std_23)
target_sources(toggle_consumer PRIVATE "${generated_named_icons}" "${generated_tab}" "${generated_tab_bar}" "${generated_menu_item}" "${generated_menu}" "${generated_submenu}" "${generated_popover}" "${generated_modal}" "${generated_alert_button}" "${generated_alert}" "${generated_sheet}" "${generated_hierarchy_item}" "${generated_hierarchy_list}" "${generated_list_toolbar}" "${generated_toolbar_button}" "${generated_list_footer}" "${generated_hierarchy_toolbar}" "${generated_table}" "${generated_color_button}")
target_sources(toggle_consumer PRIVATE "${generated_code_editor}")
target_sources(toggle_consumer PRIVATE "${generated_hierarchy_panel}")
target_sources(toggle_consumer PRIVATE "${generated_list_panel}")
target_sources(toggle_consumer PRIVATE "${generated_list_item}")
target_sources(toggle_consumer PRIVATE "${generated_list_item_composite}")
target_sources(toggle_consumer PRIVATE "${generated_list_item_actions}")
target_sources(toggle_consumer PRIVATE "${generated_list_item_stepper}")
target_sources(toggle_consumer PRIVATE "${generated_list_item_select}")
target_sources(toggle_consumer PRIVATE "${generated_card}")
target_sources(toggle_consumer PRIVATE "${generated_table_header}")
target_sources(toggle_consumer PRIVATE "${generated_table_cell_item}")
target_sources(toggle_consumer PRIVATE "${generated_table_row}")
target_include_directories(toggle_consumer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../fixtures")
target_link_libraries(toggle_consumer PRIVATE LVUI::Native iiGUI::iiGUI)
if(LVUI_CONSUMER_TEST_COLOR)
    lvui_generate(NAME NativeOwnedColorSurface SOURCE ../fixtures/NativeOwnedColorSurface.lvui
        INCLUDES NativeOwnedColorSurfaceFixture.h OUTPUT_VARIABLE generated_color_surface)
    lvui_generate(NAME NativeOwnedColorPicker SOURCE ../fixtures/NativeOwnedColorPicker.lvui
        INCLUDES NativeOwnedColorPickerFixture.h OUTPUT_VARIABLE generated_color_picker)
    target_sources(toggle_consumer PRIVATE color.cpp "${generated_color_surface}" "${generated_color_picker}")
    target_link_libraries(toggle_consumer PRIVATE LVUI::Color)
endif()
