#include "NativeStandaloneSelector.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using View=lvui_generated::NativeStandaloneSelector;
using State=ii::gui::detail::WidgetState;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
ii::gui::Widget control(const std::shared_ptr<State>& tree,std::string_view text){
    for(const auto& node:take(tree->snapshots()))if(node.id.ends_with("/control")){
        auto widget=take(tree->find(node.id));auto options=widget.action_options();
        if(options&&options->text==text)return widget;
    }
    throw std::runtime_error("Selector control not found: "+std::string(text));
}
}
int main(){
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& widgets){return View::build(widgets,{}, {},&scope);}));
    auto dynamic=control(tree,"Draft");
    auto fixed=control(tree,"Two");
    require(take(dynamic.action_options()).text=="Draft"&&take(fixed.action_options()).text=="Two","Selector inputs were not mounted");
    good(dynamic.focus());good(tree->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::down}));
    require(scope->backend_0.events==std::vector<int>{2}&&take(dynamic.action_options()).text=="Published","Selection callback or disabled option handling failed");
    scope->backend_0.index.set(0);
    scope->backend_0.options.set({{"New",true},{"Other",true}});
    auto replacement=control(tree,"New");
    require(!dynamic.snapshot()&&take(replacement.action_options()).text=="New","Reactive options did not replace the menu generation");
    scope->backend_0.index.set(7);
    require(!take(scope->binding_failures()).empty()&&take(replacement.action_options()).text=="New","Invalid selection replaced a valid selector");
    scope->backend_0.index.set(0);scope->backend_0.enabled.set(false);
    require(!replacement.focus(),"Disabled selector accepted focus");
    require(scope->backend_0.events.size()==1,"Disabled selector emitted a change");
    good(host.clear_content());require(!replacement.snapshot(),"Detached selector remained active");tree->close();
}
