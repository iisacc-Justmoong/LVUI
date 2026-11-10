#if defined(LVUI_TEST_HIERARCHY_PANEL_COUNT) && defined(LVUI_TEST_HIERARCHY_DELEGATE)
#include "NativeHierarchyPanelRowDelegate.generated.h"
#elif defined(LVUI_TEST_HIERARCHY_DELEGATE)
#include "NativeHierarchyListRowDelegate.generated.h"
#elif defined(LVUI_TEST_HIERARCHY_PANEL_COUNT)
#include "NativeHierarchyPanelCountRow.generated.h"
#else
#include "NativeHierarchyListCountRow.generated.h"
#endif
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::HierarchyResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view key,std::string_view suffix){
    for(const auto& node:take(tree->snapshots()))if(node.id.find(std::string("/key:")+std::string(key)+":")!=std::string::npos&&node.id.ends_with(suffix))return take(tree->find(node.id));
    throw std::runtime_error("Generated row widget is missing");
}
}
int main(){
#if defined(LVUI_TEST_HIERARCHY_PANEL_COUNT) && defined(LVUI_TEST_HIERARCHY_DELEGATE)
    using View=lvui_generated::NativeHierarchyPanelRowDelegate;constexpr std::string_view list_suffix="panel.list";
#elif defined(LVUI_TEST_HIERARCHY_DELEGATE)
    using View=lvui_generated::NativeHierarchyListRowDelegate;constexpr std::string_view list_suffix="tree";
#elif defined(LVUI_TEST_HIERARCHY_PANEL_COUNT)
    using View=lvui_generated::NativeHierarchyPanelCountRow;constexpr std::string_view list_suffix="panel.list";
#else
    using View=lvui_generated::NativeHierarchyListCountRow;constexpr std::string_view list_suffix="tree";
#endif
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{260,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({260,80}));auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.kind==ii::gui::WidgetKind::scroll_view&&node.id.ends_with(list_suffix);});
    require(found!=nodes.end(),"Generated hierarchy list is missing");auto list=take(tree->find(found->id));(void)take(list.snapshot());
    auto one=find(tree,"6f6e65","/row-count"),two=find(tree,"74776f","/row-count");
    auto selected=find(tree,"6f6e65","/row-selected"),one_index=find(tree,"6f6e65","/row-index"),two_index=find(tree,"74776f","/row-index");
    auto row_height=find(tree,"6f6e65","/row-height");const auto initial_height=take(row_height.snapshot()).text;
    require(take(one.snapshot()).text=="1"&&take(two.snapshot()).text=="2"&&take(selected.snapshot()).text=="false",
        "Generated count Views did not receive their own row data");
    auto identity=take(one.placement()).scope.lock();
    auto first_row=find(tree,"6f6e65","/item.row");
    require(take(scope->backend_0.controller.get()->set_row_extent("one",28.)),"Generated row resize was rejected");
    scope->backend_0.view_revision.set(1);
    require(take(first_row.action_options()).height==28&&take(row_height.snapshot()).text!=initial_height&&
            take(one.placement()).scope.lock()==identity&&
            take(scope->backend_0.source->snapshot())->revision==0,
            "Generated view revision did not resize the retained row without editing the document");
    require(take(scope->backend_0.controller.get()->set_row_extent("one",std::nullopt)),"Generated row resize could not be cleared");
    scope->backend_0.view_revision.set(2);
    require(take(first_row.action_options()).height==20&&take(row_height.snapshot()).text==initial_height,
            "Generated row did not recover its default height");
#ifdef LVUI_TEST_HIERARCHY_DELEGATE
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=100,.y=10,.buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=100,.y=10,.buttons=1}));
#else
    good(find(tree,"6f6e65","/item.row").activate());
#endif
    require(take(selected.snapshot()).text=="true","Generated row selection did not update its count View");
    auto source=scope->backend_0.source;auto entries=take(source->snapshot())->entries;std::uint64_t revision=0;
    const auto publish=[&]{revision=take(source->replace(entries,revision));scope->backend_0.revision.set(std::int64_t(revision));};
#ifdef LVUI_TEST_HIERARCHY_DELEGATE
    entries[0].count=-1;entries[0].icon_name="a-missing-unused-icon";publish();
    require(take(one.snapshot()).text=="-1"&&take(one.interaction()).visible,
        "A custom row disappeared for a negative count or resolved an unused default icon");
#endif
    entries[0].count=9;publish();
    require(take(one.snapshot()).text=="9"&&take(two.snapshot()).text=="2"&&take(one.placement()).scope.lock()==identity,
        "Updating one row changed another or recreated the retained count View");
    std::swap(entries[0],entries[1]);publish();
    good(list.set_scroll_offset(0));
    require(take(one_index.snapshot()).text=="1"&&take(two_index.snapshot()).text=="0"&&take(one.placement()).scope.lock()==identity,
        "Generated row indices or identities were lost on reorder");
    good(list.set_scroll_offset(400));require(!one.snapshot()&&!two.snapshot(),"Offscreen generated row Views were not retired");
    good(list.set_scroll_offset(0));require(take(find(tree,"6f6e65","/row-count").snapshot()).text=="9", "A remounted row lost its latest value");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();require(weak.expired(),"Generated row View retained its parent");tree->close();
}
