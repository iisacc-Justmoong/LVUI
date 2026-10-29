#include "NativeOwnedToggleSwitch.generated.h"
#include "NativeOwnedCheckBox.generated.h"
#include "NativeOwnedRadioButton.generated.h"
#include "NativeOwnedMaterial.generated.h"
#include "NativeOwnedLink.generated.h"
#include "NativeOwnedLinkMethods.generated.h"
#include "NativeOwnedPageRouter.generated.h"
#include "NativeOwnedAppCard.generated.h"
#include "NativeOwnedAppHeader.generated.h"
#include "NativeOwnedEventListener.generated.h"
#include "NativeOwnedAvatar.generated.h"
#include "NativeOwnedTooltip.generated.h"
#include "NativeOwnedTooltipInline.generated.h"
#include "NativeOwnedSlider.generated.h"
#include "NativeOwnedInputField.generated.h"
#include "NativeOwnedStepper.generated.h"
#include "NativeOwnedComboBox.generated.h"
#include "NativeOwnedSegmentedControl.generated.h"
#include "NativeOwnedHelpButton.generated.h"
#include "NativeOwnedDropdownButton.generated.h"
#include "NativeOwnedNamedIcons.generated.h"
#include "NativeOwnedTab.generated.h"
#include "NativeOwnedTabBar.generated.h"
#include "NativeOwnedMenuItem.generated.h"
#include "NativeOwnedMenu.generated.h"
#include "NativeOwnedSubmenu.generated.h"
#include "NativeOwnedPopover.generated.h"
#include "NativeOwnedModal.generated.h"
#include "NativeOwnedAlertButton.generated.h"
#include "NativeOwnedAlert.generated.h"
#include "NativeOwnedSheet.generated.h"
#include "NativeOwnedHierarchyItem.generated.h"
#include "NativeOwnedHierarchyList.generated.h"
#include "NativeOwnedListToolbar.generated.h"
#include "NativeOwnedToolbarButton.generated.h"
#include "NativeOwnedListFooter.generated.h"
#include "NativeOwnedHierarchyToolbar.generated.h"
#include "NativeOwnedHierarchyPanel.generated.h"
#include "NativeOwnedListPanel.generated.h"
#include "NativeOwnedListItem.generated.h"
#include "NativeOwnedListItemComposite.generated.h"
#include "NativeOwnedListItemActions.generated.h"
#include "NativeOwnedListItemStepper.generated.h"
#include "NativeOwnedListItemSelect.generated.h"
#include "NativeOwnedCard.generated.h"
#include "NativeOwnedTableHeader.generated.h"
#include "NativeOwnedTableCellItem.generated.h"
#include "NativeOwnedTableRow.generated.h"
#include "NativeOwnedTable.generated.h"
#include "NativeOwnedColorButton.generated.h"
#include <LVUI/Route.h>
#include <LVUI/NavigationHistory.h>
#include <LVUI/Tooltip.h>
#include <LVUI/TooltipController.h>
#include <LVUI/TooltipOutline.h>
#include <LVUI/NativeTooltipPresentation.h>
#include <LVUI/NativeTooltipScheduler.h>
#include <LVUI/NativeTooltipTarget.h>
#include <LVUI/NativeTooltip.h>
#include <LVUI/ColorSelection.h>
#include <LVUI/CodeDocument.h>
#include <LVUI/CodeViewport.h>
#include <LVUI/CodeComposition.h>
#include <LVUI/CodeEditorController.h>
#include <LVUI/CodeEditorSession.h>
#include <LVUI/NativeCodeEditor.h>
#include <LVUI/NativeHierarchyPanel.h>
#include <LVUI/List.h>
#include <LVUI/ListViewport.h>
#include <LVUI/NativeListView.h>
#include "NativeOwnedCodeEditor.generated.h"

// Keep the generated factory in the linked consumer; no window is opened.
auto* volatile toggle_factory = &lvui_generated::NativeOwnedToggleSwitch::build;
auto* volatile checkbox_factory = &lvui_generated::NativeOwnedCheckBox::build;
auto* volatile radio_factory = &lvui_generated::NativeOwnedRadioButton::build;
auto* volatile material_factory = &lvui_generated::NativeOwnedMaterial::build;
auto* volatile link_factory = &lvui_generated::NativeOwnedLink::build;
auto* volatile methods_factory = &lvui_generated::NativeOwnedLinkMethods::build;
auto* volatile page_router_factory = &lvui_generated::NativeOwnedPageRouter::build;
auto* volatile card_factory = &lvui_generated::NativeOwnedAppCard::build;
auto* volatile header_factory = &lvui_generated::NativeOwnedAppHeader::build;
auto* volatile listener_factory = &lvui_generated::NativeOwnedEventListener::build;
auto* volatile avatar_factory = &lvui_generated::NativeOwnedAvatar::build;
auto* volatile tooltip_generated_factory = &lvui_generated::NativeOwnedTooltip::build;
auto* volatile tooltip_inline_factory = &lvui_generated::NativeOwnedTooltipInline::build;
auto* volatile slider_generated_factory = &lvui_generated::NativeOwnedSlider::build;
auto* volatile input_field_generated_factory = &lvui_generated::NativeOwnedInputField::build;
auto* volatile stepper_generated_factory = &lvui_generated::NativeOwnedStepper::build;
auto* volatile combo_box_generated_factory = &lvui_generated::NativeOwnedComboBox::build;
auto* volatile segmented_generated_factory = &lvui_generated::NativeOwnedSegmentedControl::build;
auto* volatile help_generated_factory = &lvui_generated::NativeOwnedHelpButton::build;
auto* volatile dropdown_generated_factory = &lvui_generated::NativeOwnedDropdownButton::build;
auto* volatile named_icons_factory = &lvui_generated::NativeOwnedNamedIcons::build;
auto* volatile tab_factory = &lvui_generated::NativeOwnedTab::build;
auto* volatile tab_bar_factory = &lvui_generated::NativeOwnedTabBar::build;
auto* volatile menu_item_factory = &lvui_generated::NativeOwnedMenuItem::build;
auto* volatile menu_factory = &lvui_generated::NativeOwnedMenu::build;
auto* volatile submenu_factory = &lvui_generated::NativeOwnedSubmenu::build;
auto* volatile popover_factory = &lvui_generated::NativeOwnedPopover::build;
auto* volatile modal_factory = &lvui_generated::NativeOwnedModal::build;
auto* volatile alert_button_factory = &lvui_generated::NativeOwnedAlertButton::build;
auto* volatile alert_factory = &lvui_generated::NativeOwnedAlert::build;
auto* volatile sheet_factory = &lvui_generated::NativeOwnedSheet::build;
auto* volatile hierarchy_item_factory = &lvui_generated::NativeOwnedHierarchyItem::build;
auto* volatile hierarchy_list_factory = &lvui_generated::NativeOwnedHierarchyList::build;
auto* volatile list_toolbar_factory = &lvui_generated::NativeOwnedListToolbar::build;
auto* volatile toolbar_button_factory = &lvui_generated::NativeOwnedToolbarButton::build;
auto* volatile list_footer_factory = &lvui_generated::NativeOwnedListFooter::build;
auto* volatile hierarchy_toolbar_factory = &lvui_generated::NativeOwnedHierarchyToolbar::build;
auto* volatile hierarchy_panel_factory = &lvui::native::NativeHierarchyPanel::create;
auto* volatile list_model_factory = &lvui::ListMemory::create;
auto* volatile list_viewport_factory = &lvui::ListViewportSource::create;
auto* volatile list_view_factory = &lvui::native::NativeListView::create;
auto* volatile generated_list_panel_factory = &lvui_generated::NativeOwnedListPanel::build;
auto* volatile generated_list_item_factory = &lvui_generated::NativeOwnedListItem::build;
auto* volatile generated_list_item_composite_factory = &lvui_generated::NativeOwnedListItemComposite::build;
auto* volatile generated_list_item_actions_factory = &lvui_generated::NativeOwnedListItemActions::build;
auto* volatile generated_list_item_stepper_factory = &lvui_generated::NativeOwnedListItemStepper::build;
auto* volatile generated_list_item_select_factory = &lvui_generated::NativeOwnedListItemSelect::build;
auto* volatile generated_card_factory = &lvui_generated::NativeOwnedCard::build;
auto* volatile generated_table_header_factory = &lvui_generated::NativeOwnedTableHeader::build;
auto* volatile generated_table_cell_item_factory = &lvui_generated::NativeOwnedTableCellItem::build;
auto* volatile generated_table_row_factory = &lvui_generated::NativeOwnedTableRow::build;
auto* volatile generated_hierarchy_panel_factory = &lvui_generated::NativeOwnedHierarchyPanel::build;
auto* volatile code_document_factory = &lvui::CodeMemoryDocument::create;
auto* volatile code_controller_factory = &lvui::CodeEditorController::create;
auto* volatile code_composition_factory = &lvui::CodeComposition::create;
auto* volatile code_session_factory = &lvui::CodeEditorSession::create;
auto* volatile native_code_editor_factory = &lvui::native::NativeCodeEditor::create;
auto* volatile generated_code_editor_factory = &lvui_generated::NativeOwnedCodeEditor::build;
auto* volatile table_factory = &lvui_generated::NativeOwnedTable::build;
auto* volatile color_button_factory = &lvui_generated::NativeOwnedColorButton::build;
lvui::ColorSelectionResult<lvui::ColorEditToken> begin_color(lvui::ColorSelection& selection){return selection.begin();}
auto* volatile color_selection_factory = &begin_color;
ii::core::Result<std::shared_ptr<const lvui::RouteResolution>> resolve_route(lvui::RouteResolver& resolver){return resolver.resolve("/example");}
auto* volatile route_factory=&resolve_route;
ii::core::Result<bool> commit_history(lvui::NavigationHistory& history,const lvui::NavigationPlan& plan){return history.commit(plan);}
ii::core::Result<lvui::NavigationPlan> prepare_history(lvui::NavigationHistory& history){return history.prepare(lvui::NavigationOperation::pop);}
auto* volatile history_factory=&commit_history;
auto* volatile history_plan_factory=&prepare_history;
std::optional<lvui::TooltipLayout> tooltip_layout(lvui::Tooltip& tooltip){return tooltip.layout({0,0,200,100},{100,50},{40,20});}
auto* volatile tooltip_factory=&tooltip_layout;
auto* volatile tooltip_controller_factory=&lvui::TooltipController::create;
auto* volatile tooltip_outline_factory=&lvui::tooltip_outline;
auto* volatile tooltip_presentation_factory=&lvui::native::NativeTooltipPresentation::create;
auto* volatile tooltip_scheduler_factory=&lvui::native::NativeTooltipScheduler::create;
auto tooltip_scheduler_port(const lvui::native::NativeTooltipScheduler& scheduler){return scheduler.weak_schedule();}
auto* volatile tooltip_scheduler_port_factory=&tooltip_scheduler_port;
auto* volatile tooltip_target_factory=&lvui::native::NativeTooltipTarget::create;
auto* volatile tooltip_native_factory=&lvui::native::NativeTooltip::create;
int main(){return toggle_factory&&checkbox_factory&&radio_factory&&material_factory&&link_factory&&methods_factory&&page_router_factory&&card_factory&&header_factory&&listener_factory&&avatar_factory&&tooltip_generated_factory&&tooltip_inline_factory&&slider_generated_factory&&input_field_generated_factory&&stepper_generated_factory&&combo_box_generated_factory&&segmented_generated_factory&&help_generated_factory&&dropdown_generated_factory&&named_icons_factory&&tab_factory&&tab_bar_factory&&menu_item_factory&&menu_factory&&submenu_factory&&popover_factory&&modal_factory&&alert_button_factory&&alert_factory&&sheet_factory&&hierarchy_item_factory&&hierarchy_list_factory&&list_toolbar_factory&&table_factory&&color_button_factory&&color_selection_factory&&route_factory&&history_factory&&history_plan_factory&&tooltip_factory&&tooltip_controller_factory&&tooltip_outline_factory&&tooltip_presentation_factory&&tooltip_scheduler_factory&&tooltip_scheduler_port_factory&&tooltip_target_factory&&tooltip_native_factory?0:1;}
