#include "NativeOwnedListRowDelegate.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view key,std::string_view suffix){
    for(const auto& node:take(tree->snapshots()))if(node.id.find(std::string("/key:")+std::string(key)+":")!=std::string::npos&&node.id.ends_with(suffix))return take(tree->find(node.id));
    throw std::runtime_error("Generated flat-list row widget is missing: "+std::string(suffix));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListRowDelegate;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({240,160}));
    auto alpha=find(tree,"61","/custom-label"),beta=find(tree,"62","/custom-label");
    auto selected=find(tree,"61","/custom-selected"),beta_selected=find(tree,"62","/custom-selected");
    auto alpha_index=find(tree,"61","/custom-index"),beta_index=find(tree,"62","/custom-index");
    require(take(alpha.snapshot()).text=="Alpha"&&take(beta.snapshot()).text=="Beta"&&take(selected.snapshot()).text=="false",
            "Generated custom rows did not receive independent values");
    auto shared=find(tree,"61","/shared-label");require(take(shared.snapshot()).text=="Shared", "A delegate without row binding was not mounted");
    auto identity=take(alpha.placement()).scope.lock();auto row=find(tree,"61","/row"),beta_row=find(tree,"62","/row");
    ii::gui::Widget list_widget;
    for(const auto& node:take(tree->snapshots()))if(node.id.ends_with("/list.list"))list_widget=take(tree->find(node.id));
    require(!list_widget.is_null(),"The generated list viewport is missing");
    require(take(row.snapshot()).bounds.height==30,"A generated delegate's natural height was ignored");
    auto trigger=find(tree,"61","/custom-trigger");good(trigger.activate());
    require(take(selected.snapshot()).text=="true"&&scope->backend_0.activations==1,
            "A generated nested button did not activate its own row exactly once");
    scope->backend_0.row_height.set(40);(void)take(list_widget.snapshot());
    auto first_bounds=take(row.snapshot()).bounds,second_bounds=take(beta_row.snapshot()).bounds;
    if(first_bounds.height!=40||second_bounds.y!=first_bounds.y+40||take(alpha.placement()).scope.lock()!=identity){
        auto content=find(tree,"61","/delegate.content");
        throw std::runtime_error("A reactive delegate height did not reflow retained generated rows: height="+
            std::to_string(first_bounds.height)+" first="+std::to_string(first_bounds.y)+" second="+std::to_string(second_bounds.y)+
            " button="+std::to_string(take(trigger.action_options()).height.value_or(-1))+" measured="+std::to_string(take(content.measure(200)).height)+
            " action="+std::to_string(take(row.action_options()).height.value_or(-1))+" after="+std::to_string(take(row.snapshot()).bounds.height));
    }
    scope->backend_0.row_height.set(30);(void)take(list_widget.snapshot());
    auto controller=scope->backend_0.controller.get();require(take(controller->select("b")),"External list selection was rejected");
    scope->backend_0.view_revision.set(std::int64_t(take(controller->view_revision())));
    require(take(selected.snapshot()).text=="false"&&take(beta_selected.snapshot()).text=="true"&&
            take(scope->backend_0.source->snapshot())->revision==0&&scope->backend_0.activations==1,
            "View-only selection did not refresh delegates independently of document revision and callbacks");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated row diagnostics are unavailable");
    const auto view_revision=scope->backend_0.view_revision.get();scope->backend_0.view_revision.set(-1);
    require(!take(graph->failures()).empty()&&take(beta_selected.snapshot()).text=="true","Invalid view revision replaced the current rows");
    scope->backend_0.view_revision.set(view_revision);require(take(graph->failures()).empty(),"A valid view revision did not recover the row binding");
    auto source=scope->backend_0.source;auto item=take(source->snapshot())->entries[0];item.label="Changed";
    auto revision=take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="a",.item=item}));scope->backend_0.revision.set(std::int64_t(revision));
    require(take(alpha.snapshot()).text=="Changed"&&take(beta.snapshot()).text=="Beta"&&take(alpha.placement()).scope.lock()==identity,
            "Generated row update recreated its View or changed another row");
    require(take(shared.snapshot()).text=="Shared","A delegate without row binding lost its external scope");
    revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::move,.key="b",.before="a"}));scope->backend_0.revision.set(std::int64_t(revision));
    require(take(alpha_index.snapshot()).text=="1"&&take(beta_index.snapshot()).text=="0"&&take(alpha.placement()).scope.lock()==identity,
            "Generated row reordering lost its identity or source index");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();require(weak.expired()&&!alpha.snapshot(),"Generated rows retained a closed scope");tree->close();
}
