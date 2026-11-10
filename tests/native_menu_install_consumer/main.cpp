#include "NativeMenuBundles.generated.h"
#include "NativeMenuNestedComposition.generated.h"
#include "NativeTableStructureControls.generated.h"
#include "NativeTabDynamic.generated.h"
#include "NativeOwnedSearchField.generated.h"
#include "NativeOwnedSlider.generated.h"
#include "NativeFocusRing.generated.h"
#include "NativeAbstractInputBar.generated.h"
#include "NativeExplicitListItemComposite.generated.h"
#include "NativeMobileTabs.generated.h"
#include "NativeWindowNoise.generated.h"
#include "NativeWheelGuard.generated.h"

// Keep the generated factory linkable without opening a native window.
auto* volatile menu_bundle_factory = &lvui_generated::NativeMenuBundles::build;
auto* volatile nested_menu_factory = &lvui_generated::NativeMenuNestedComposition::build;
auto* volatile table_structure_factory = &lvui_generated::NativeTableStructureControls::build;
auto* volatile table_structure_planner = &lvui::plan_table_structure_action;
auto* volatile dynamic_tab_factory = &lvui_generated::NativeTabDynamic::build;
auto* volatile search_field_factory = &lvui_generated::NativeOwnedSearchField::build;
auto* volatile slider_factory = &lvui_generated::NativeOwnedSlider::build;
auto* volatile focus_ring_factory = &lvui_generated::NativeFocusRing::build;
auto* volatile input_bar_factory = &lvui_generated::NativeAbstractInputBar::build;
auto* volatile explicit_composite_factory = &lvui_generated::NativeExplicitListItemComposite::build;
auto* volatile mobile_tabs_factory = &lvui_generated::NativeMobileTabs::build;
auto* volatile noise_factory = &lvui_generated::NativeWindowNoise::build;
auto* volatile wheel_factory = &lvui_generated::NativeWheelGuard::build;
static_assert(requires(const lvui_generated::NativeTabDynamic::Scope& scope){scope.binding_failures();});
static_assert(requires(const lvui_generated::NativeTabDynamic& view){view.binding_failures();});
static_assert(lvui::TableStructureAction::insert_first_row != lvui::TableStructureAction::insert_row_after);
static_assert(lvui::TableStructureAction::insert_first_column != lvui::TableStructureAction::insert_column_after);
int main(){return menu_bundle_factory && nested_menu_factory && table_structure_factory && table_structure_planner && dynamic_tab_factory && search_field_factory && slider_factory && focus_ring_factory && input_bar_factory && explicit_composite_factory && mobile_tabs_factory && noise_factory && wheel_factory ? 0 : 1;}
