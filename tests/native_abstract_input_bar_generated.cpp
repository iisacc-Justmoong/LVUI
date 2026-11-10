#include "NativeAbstractInputBar.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::string text(ii::gui::Widget field){auto s=take(field.snapshot());return take(field.text_content({0,s.text_length})).text;}
}
int main(){
    using View=lvui_generated::NativeAbstractInputBar;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/field");});
    require(node!=nodes.end(),"Generated input bar is missing");auto field=take(tree->find(node->id));
    require(take(field.snapshot()).text_input&&text(field)=="Draft","Input bar did not mount an editable iiGUI field");
    good(field.focus());good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    require(scope->backend_0.text.get()=="Draft!"&&text(field)=="Draft!","User input did not update the bound source");
    scope->backend_0.text.set("External");require(text(field)=="External","Source update did not reach the input bar");
    std::weak_ptr<View::Scope> lifetime=scope;good(host.clear_content());scope.reset();
    require(lifetime.expired()&&!field.snapshot(),"Retired input bar retained its scope");tree->close();
}
