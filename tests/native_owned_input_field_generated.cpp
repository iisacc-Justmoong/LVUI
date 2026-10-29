#include "NativeOwnedInputField.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::string text(ii::gui::Widget field){auto snapshot=take(field.snapshot());return take(field.text_content({0,snapshot.text_length})).text;}
}
int main(){
    using View=lvui_generated::NativeOwnedInputField;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/field");});
    require(found!=nodes.end(),"Generated input field is missing");auto field=take(tree->find(found->id));auto initial=take(field.snapshot());
    require(initial.text_input&&initial.kind==WidgetKind::absolute_stack&&text(field)=="Draft","Generated field is not LVUI-owned");
    good(field.focus());good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(text(field)=="Draft한"&&scope->backend_0.text.get()=="Draft한"&&scope->backend_0.edits.size()==1,
            "Generated field lost its edit callback or reactive reentry");
    scope->backend_0.width.set(200);scope->backend_0.placeholder.set("Other");
    require(text(field)=="Draft한"&&take(field.snapshot()).size.width==200,"Reactive options replaced the field or its edit");
    scope->backend_0.text.set("External");require(text(field)=="External","Reactive field text did not update");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(-1);scope->backend_0.text.set("Later");
    require(!take(graph->failures()).empty()&&text(field)=="External","Invalid options replaced the retained field");
    scope->backend_0.width.set(180);require(take(graph->failures()).empty()&&text(field)=="Later","Field did not recover after invalid options");
    scope->backend_0.password.set(true);require(take(field.snapshot()).password,"Reactive secure state did not reach the host");
    scope->backend_0.readOnly.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    require(text(field)=="Later","Read-only generated field accepted input");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));require(scope->backend_0.submissions==1,"Generated field lost submission");
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::text,.text="y"}));
    require(text(field)=="Later","Disabled generated field accepted input");
    good(host.clear_content());scope.reset();graph.reset();require(!field.snapshot(),"Retired generated field remained active");tree->close();
}
