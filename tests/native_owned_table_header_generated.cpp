#include "NativeOwnedTableHeader.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated table header did not mount");return take(tree->find(node->id));
}
std::size_t images(const ii::gui::CanvasFrame& frame){std::size_t count=frame.images.size();for(const auto& layer:frame.overlays)count+=layer.images.size();return count;}
}
int main(){
    using View=lvui_generated::NativeOwnedTableHeader;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto header=find(tree,"/header");auto root=take(header.snapshot());
    require(images(take(tree->drawing_snapshot({350,80})))==3,"Generated header lost default leading icons");
    auto source=lvui::IconCatalog::create({{"generalchevronDown","<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\"><rect width=\"18\" height=\"18\" fill=\"#f00\"/></svg>"}});
    require(bool(source),"Generated header icon source fixture failed");
    scope->backend_0.iconSource.set(*source);scope->backend_0.iconRevision.set(1);
    require(images(take(tree->drawing_snapshot({350,80})))==3,"Generated header did not reactively accept an icon source");
    scope->backend_0.columnAppearance.set({{.column="1",.leading_icon=""}});
    require(images(take(tree->drawing_snapshot({350,80})))==2,"Generated header did not reactively hide a column icon");
    auto diagnostics=scope->native_lifetime.lock();
    scope->backend_0.columnAppearance.set({{.column="1",.leading_icon=""},{.column="1",.leading_icon=""}});
    require(diagnostics&&!take(diagnostics->failures()).empty()&&images(take(tree->drawing_snapshot({350,80})))==2,
        "Invalid generated column appearance did not preserve the last good header");
    scope->backend_0.columnAppearance.set({});scope->backend_0.defaultLeadingIcon.set("");
    require(take(diagnostics->failures()).empty()&&images(take(tree->drawing_snapshot({350,80})))==0,"Generated header did not recover and clear default icons");
    scope->backend_0.defaultLeadingIcon.set("generalchevronDown");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(scope->backend_0.clicked==std::vector<std::string>{"Second"},"Generated header lost the clicked column");
    scope->backend_0.columns.set({"One","Two","Three"});
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(scope->backend_0.clicked.back()=="Two","Generated header did not update its columns");
    auto text=lvui::TableText{};text.header_group="열 머리글";text.unnamed_column="{column}열";
    scope->backend_0.tableText.set(text);scope->backend_0.columns.set({"One","Two",""});
    auto accessible=take(tree->accessibility_snapshots());
    require(take(header.snapshot()).accessibility_semantics->name=="열 머리글"&&
        std::ranges::any_of(accessible,[](const auto& value){return value.accessibility_semantics&&
            value.accessibility_semantics->role==AccessibilityRole::column_header&&value.accessibility_name=="3열";}),
        "Generated header did not reactively localize its group and unnamed column");
    scope->backend_0.sort.set(lvui::TableHeaderSort{"1",lvui::TableSortOrder::descending});
    accessible=take(tree->accessibility_snapshots());
    require(std::ranges::any_of(accessible,[&](const auto& value){return value.accessibility_name=="Two"&&
        value.accessibility_description==text.sorted_descending;}),"Generated header did not reactively expose sort state");
    auto graph=scope->native_lifetime.lock();
    scope->backend_0.sort.set(lvui::TableHeaderSort{"missing",lvui::TableSortOrder::ascending});
    accessible=take(tree->accessibility_snapshots());
    require(graph&&!take(graph->failures()).empty()&&std::ranges::any_of(accessible,[&](const auto& value){
        return value.accessibility_name=="Two"&&value.accessibility_description==text.sorted_descending;}),
        "Invalid reactive sort did not retain the last good indication");
    scope->backend_0.sort.set(lvui::TableHeaderSort{"1",lvui::TableSortOrder::ascending});
    require(take(graph->failures()).empty(),"Header did not recover after invalid reactive sort");
    scope->backend_0.sort.set(std::nullopt);
    scope->backend_0.columns.set({"One","Two","Three"});
    scope->backend_0.columns.set({"Only"});
    require(graph&&!take(graph->failures()).empty(),"Mismatched reactive column widths were accepted");
    scope->backend_0.columns.set({"One","Two","Three"});require(take(graph->failures()).empty(),"Header did not recover after invalid columns");
    scope->backend_0.visible.set(false);require(!take(header.interaction()).visible,"Reactive header visibility was ignored");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();diagnostics.reset();
    require(weak.expired()&&!header.snapshot(),"Generated header retained its owner");tree->close();
}
