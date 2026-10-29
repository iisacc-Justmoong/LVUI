#include "NativeOwnedTabBar.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated TabBar child did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedTabBar;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto group=find(tree,"/tabs"),first=find(tree,"/first"),disabled=find(tree,"/disabled"),last=find(tree,"/last");
    (void)take(tree->drawing_snapshot({500,180}));
    const auto group_info=take(group.snapshot());
    require(group_info.accessibility_name=="Sections"&&group_info.accessibility_semantics&&group_info.accessibility_semantics->role==AccessibilityRole::tab_group&&
            take(first.snapshot()).selected&&!take(disabled.snapshot()).enabled&&!take(last.snapshot()).selected,
            "Generated TabBar initial selection is incorrect");
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(last.snapshot()).focused&&take(first.snapshot()).selected,"Generated TabBar keyboard focus is incorrect");
    good(last.activate());require(scope->backend_0.selections==std::vector<int>({2})&&scope->backend_0.clicks==1&&
        take(last.snapshot()).selected&&!take(first.snapshot()).selected,"Generated TabBar activation is incorrect");
    scope->backend_0.lastVisible.set(false);
    auto first_after_hide=take(first.snapshot()),last_after_hide=take(last.snapshot());
    if(!(!last_after_hide.visible&&first_after_hide.selected&&first_after_hide.bounds.width>=120&&
         scope->backend_0.selections==std::vector<int>({2,0})))
        throw std::runtime_error("Hidden selected tab: visible="+std::to_string(last_after_hide.visible)+
            " selected="+std::to_string(first_after_hide.selected)+" width="+std::to_string(first_after_hide.bounds.width)+
            " callbacks="+std::to_string(scope->backend_0.selections.size())+
            " model_index="+std::to_string(scope->backend_0.index.get()));
    scope->backend_0.lastVisible.set(true);good(last.activate());
    require(take(last.snapshot()).selected&&scope->backend_0.selections==std::vector<int>({2,0,2})&&scope->backend_0.clicks==2,
            "A visible TabBar child did not recover");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated TabBar graph is unavailable");
    scope->backend_0.index.set(1);require(!take(graph->failures()).empty()&&take(last.snapshot()).selected,
        "Invalid reactive selection replaced the live TabBar");
    scope->backend_0.index.set(0);require(take(graph->failures()).empty()&&take(first.snapshot()).selected,
        "Generated TabBar did not recover from an invalid selection");
    scope->backend_0.middleEnabled.set(true);scope->backend_0.index.set(1);
    require(take(disabled.snapshot()).selected&&take(disabled.snapshot()).enabled,"Enabled TabBar child could not be selected");
    scope->backend_0.middleEnabled.set(false);
    require(take(first.snapshot()).selected&&!take(disabled.snapshot()).selected&&scope->backend_0.selections==std::vector<int>({2,0,2,0}),
            "TabBar did not move selection after the selected child became disabled");
    scope->backend_0.automatic.set(false);scope->backend_0.index.set(0);good(last.activate());
    require(scope->backend_0.clicks==3&&scope->backend_0.selections==std::vector<int>({2,0,2,0}),"Manual TabBar activated an automatic selection callback");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!first.snapshot()&&!last.snapshot(),"Generated TabBar retained its activation");tree->close();
}
