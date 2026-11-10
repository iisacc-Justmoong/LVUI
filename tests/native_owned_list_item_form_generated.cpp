#include "NativeOwnedListItemForm.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix,std::string_view text={}){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix)&&(text.empty()||value.text==text);});
    if(node==nodes.end())throw std::runtime_error("Generated form control was not mounted: "+std::string(suffix)+" / "+std::string(text));
    return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemForm;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,230}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,230}));
    auto first=find(tree,"/form.input1"),second=find(tree,"/form.input2");
    good(first.focus());good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    good(second.focus());good(tree->input({.kind=WidgetInputKind::text,.text="?"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(find(tree,"form.actions.slot1","Save").activate());
    require(scope->backend_0.title.get()=="Title!"&&scope->backend_0.notes.get()=="Notes?"&&
        scope->backend_0.events==std::vector<std::string>({"title:Title!","edit:inputText1","notes:Notes?","edit:inputText2","submit:Notes?",
            "event:inputSubmitted:inputText2:Notes?","values:primary:Title!:Notes?:1","event:primary:Title!:Notes?"}),
        "Generated form did not preserve independent field events");
    scope->backend_0.label2.set("Memo");require(take(second.snapshot()).accessibility_name=="Memo","Reactive second field label did not update accessibility");
    scope->backend_0.read_only2.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="X"}));
    require(scope->backend_0.notes.get()=="Notes?","Generated read-only second field changed");
    scope->backend_0.kind.set(lvui::ListItemVariant::task);(void)take(tree->drawing_snapshot({500,230}));
    require(!take(second.interaction()).visible,"Second input remained visible after changing form type");
    scope->backend_0.kind.set(lvui::ListItemVariant::form);(void)take(tree->drawing_snapshot({500,230}));
    require(take(second.interaction()).visible&&scope->backend_0.notes.get()=="Notes?","Returning to form lost the second field");
    (void)find(tree,"/control","Category");(void)find(tree,"form.actions.slot0","Cancel");(void)find(tree,"form.actions.slot1","Save");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated form retained its owner");tree->close();
}
