#ifdef LVUI_TEST_HIERARCHY_PANEL_COUNT
#include "NativeHierarchyPanelCountView.generated.h"
#else
#include "NativeHierarchyListCountView.generated.h"
#endif
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::HierarchyResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::vector<ii::gui::Widget> labels(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view text){
    std::vector<ii::gui::Widget> result;
    for(const auto& node:take(tree->snapshots()))if(node.text==text)result.push_back(take(tree->find(node.id)));
    return result;
}
}
int main(){
#ifdef LVUI_TEST_HIERARCHY_PANEL_COUNT
    using View=lvui_generated::NativeHierarchyPanelCountView;
    constexpr std::string_view list_suffix="panel.list";
#else
    using View=lvui_generated::NativeHierarchyListCountView;
    constexpr std::string_view list_suffix="tree";
#endif
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{260,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({260,80}));
    auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.kind==ii::gui::WidgetKind::scroll_view&&node.id.ends_with(list_suffix);});
    require(found!=nodes.end(),"Generated hierarchy list is missing");auto list=take(tree->find(found->id));
    (void)take(list.snapshot());auto badges=labels(tree,"Badge");
    std::set<std::string> ids;for(const auto& badge:badges)ids.insert(take(badge.snapshot()).id);
    require(badges.size()>=2&&badges.size()<32&&ids.size()==badges.size(),
        "Generated count Views did not mount once per visible row");
    scope->backend_0.badge.set("Updated");
    require(labels(tree,"Updated").size()==badges.size()&&
            std::ranges::all_of(badges,[](const auto& badge){return take(badge.snapshot()).text=="Updated";}),
        "Generated count Views lost their shared reactive input or identity");
    auto source=scope->backend_0.source;auto entries=take(source->snapshot())->entries;entries[0].count=-1;
    scope->backend_0.revision.set(std::int64_t(take(source->replace(entries,0))));
    const auto visible=std::ranges::count_if(badges,[](const auto& badge){return take(badge.interaction()).visible;});
    require(visible==std::ptrdiff_t(badges.size()-1),"Generated count View visibility did not follow its own row");
    good(list.set_scroll_offset(400));
    require(std::ranges::all_of(badges,[](const auto& badge){return !badge.snapshot();})&&!labels(tree,"Updated").empty(),
        "Virtualized generated count Views did not retire or remount");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();
    require(weak.expired(),"Generated count Views retained a retired scope");tree->close();
}
