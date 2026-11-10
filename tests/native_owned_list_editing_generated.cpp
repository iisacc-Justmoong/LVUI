#include "NativeOwnedListEditing.generated.h"
#include <iiGUI/WidgetState.h>

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
    for(const auto& node:take(tree->snapshots()))if(node.id.find(std::string("/key:")+std::string(key)+":")!=std::string::npos&&node.id.ends_with(suffix))
        return take(tree->find(node.id));
    throw std::runtime_error("Generated list edit widget not found");
}
}
int main(){
    using View=lvui_generated::NativeOwnedListEditing;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{220,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({220,80}));auto row=find(tree,"61","/row");good(row.activate());
    const auto key=[&](WidgetKey value){good(tree->input({.kind=WidgetInputKind::key_down,.key=value}));};
    const auto replace=[&](std::string text){key(WidgetKey::select_all);good(tree->input({.kind=WidgetInputKind::text,.text=std::move(text)}));};
    key(WidgetKey::enter);auto field=find(tree,"61","/field");replace("Generated");
    require(scope->backend_0.edited_count==1&&scope->backend_0.key=="a"&&scope->backend_0.field=="label"&&
        scope->backend_0.text=="Generated"&&take(scope->backend_0.source->snapshot())->revision==0,
        "Generated list editing lost its row callback or changed the source early");
    key(WidgetKey::enter);require(scope->backend_0.submitted_count==1&&scope->backend_0.revision.get()==1&&
        take(scope->backend_0.source->snapshot())->entries[0].label=="Generated"&&!field.snapshot(),
        "Generated list submission was not committed exactly once");
    key(WidgetKey::enter);replace("Pending");auto item=take(scope->backend_0.source->snapshot())->entries[0];item.label="Newer";
    const auto revision=take(scope->backend_0.source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key="a",.item=item}));
    scope->backend_0.revision.set(std::int64_t(revision));key(WidgetKey::enter);
    require(scope->backend_0.rejected_count==1&&scope->backend_0.text=="Pending"&&!scope->backend_0.error.empty()&&
        take(scope->backend_0.source->snapshot())->entries[0].label=="Newer","Generated list did not report a stale edit");
    key(WidgetKey::escape);scope->backend_0.editable.set(false);key(WidgetKey::enter);
    bool found=false;for(const auto& node:take(tree->snapshots()))if(node.id.ends_with("/field"))found=true;
    require(!found&&scope->backend_0.submitted_count==1,"Reactive list editing disablement still opened an editor");
    good(host.clear_content());tree->close();
}
