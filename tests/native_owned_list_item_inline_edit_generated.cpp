#include "NativeOwnedListItemInlineEdit.generated.h"
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
    if(node==nodes.end())throw std::runtime_error("Generated inline field did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemInlineEdit;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto field=find(tree,"/item.input1");good(field.focus());good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    require(scope->backend_0.changes==std::vector<std::string>{"Name!"}&&scope->backend_0.edits==std::vector<std::string>{"inputText1"}&&
            scope->backend_0.text.get()=="Name!","Generated inline field lost its reactive edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));require(scope->backend_0.submissions==std::vector<std::string>{"Name!"},"Generated inline submission was lost");
    scope->backend_0.read_only.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="X"}));require(scope->backend_0.changes.size()==1,"Read-only generated inline field changed");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!field.snapshot(),"Generated inline field retained its owner");tree->close();
}
