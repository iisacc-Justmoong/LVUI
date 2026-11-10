#include "NativeOwnedListItemContext.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Contextual list item view was not mounted");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemContext;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,1000}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,1000}));
    auto first=find(tree,"/first"),label=find(tree,"/context-label"),checked=find(tree,"/context-checked");
    auto edit=find(tree,"/context-edit"),step=find(tree,"/context-step"),action=find(tree,"/context-action"),second=find(tree,"/second-label");
    require(take(label.snapshot()).text=="Alpha"&&take(checked.snapshot()).text=="false"&&take(second.snapshot()).text=="Second",
            "A forwarded contextual view did not receive its own list item state");
    for(auto id:{"/slot-leading","/slot-trailing","/slot-footer"})
        require(take(find(tree,id).snapshot()).text=="Slots","A custom slot received another item's context");
    require(take(find(tree,"/slot-preview").snapshot()).text=="Preview","The preview did not receive its item context");
    require(take(find(tree,"/outer-before").snapshot()).text=="Outer"&&take(find(tree,"/outer-after").snapshot()).text=="Outer"&&
            take(find(tree,"/inner-label").snapshot()).text=="Inner","Nested list item contexts did not respect lexical scope");
    for(auto suffix:{"/delegate-owner","/delegate-row"}){
        std::vector<std::string> labels;
        for(const auto& node:take(tree->snapshots()))if(node.id.ends_with(suffix))labels.push_back(node.text);
        std::ranges::sort(labels);
        require(labels==std::vector<std::string>{"Row A","Row B"},"List row and list item scopes did not coexist");
    }
    auto builtin=find(tree,"/builtin.checkbox"),builtin_checked=find(tree,"/builtin-checked");
    require(take(builtin_checked.snapshot()).text=="false","Built-in check context has the wrong initial value");
    good(builtin.activate());(void)take(find(tree,"/builtin").snapshot());
    require(take(builtin_checked.snapshot()).text=="true"&&take(find(tree,"/builtin-active").snapshot()).text=="Builtin",
            "A built-in control did not publish its change into a conditional custom view");
    const auto repetitions=[&](std::string_view expected,std::size_t count){
        auto nodes=take(tree->snapshots());std::size_t found=0;
        for(const auto& node:nodes)if(node.id.ends_with("/context-repeat")){++found;require(node.text==expected,"A repeated view lost its owner context");}
        require(found==count,"Unexpected number of contextual repeated views");
    };
    repetitions("Alpha",2);
    scope->backend_0.label.set("Changed");(void)take(first.snapshot());
    (void)take(find(tree,"/loop").snapshot());repetitions("Changed",2);
    require(take(label.snapshot()).text=="Changed"&&take(second.snapshot()).text=="Second",
            "A contextual value did not update independently");
    scope->backend_0.entries.set({{"second"},{"third"}});(void)take(find(tree,"/loop").snapshot());repetitions("Changed",2);
    good(edit.activate());(void)take(first.snapshot());
    require(scope->backend_0.checked.get()&&take(checked.snapshot()).text=="true"&&scope->backend_0.edits.size()==1&&
            scope->backend_0.edits.back().first=="checked", "The contextual edit did not update the owner and model");
    good(step.activate());(void)take(first.snapshot());
    require(scope->backend_0.quantity.get()==2&&scope->backend_0.edits.size()==2&&scope->backend_0.edits.back().first=="quantity",
            "The contextual step did not update the owner and model");
    good(action.activate());require(scope->backend_0.actions==1,"The contextual action did not reach the owner callback");
    good(find(tree,"/payload-action").activate());scope->backend_0.payload_value.set("asset-43");
    good(find(tree,"/payload-action").activate());require(scope->backend_0.payload_actions==2&&
            scope->backend_0.payload_values==std::vector<std::string>({"asset-42","asset-43"}),
            "The contextual action did not follow its reactive payload");
    good(find(tree,"/context-number").activate());good(find(tree,"/context-segment").activate());good(find(tree,"/context-text").activate());
    require(scope->backend_0.quantity.get()==5&&scope->backend_0.edits.size()==5&&
            scope->backend_0.edits[3].first=="segmentIndex"&&std::get<std::int64_t>(scope->backend_0.edits[3].second)==1&&
            scope->backend_0.edits[4].first=="inputText1"&&std::get<std::string>(scope->backend_0.edits[4].second)=="Hello",
            "Typed contextual commands did not forward numeric, index, and text edits");
    scope->backend_0.checked.set(false);(void)take(first.snapshot());
    require(take(checked.snapshot()).text=="false", "The contextual view did not follow an external state change");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!label.snapshot()&&!edit.snapshot(),"Contextual views retained a retired owner");tree->close();
}
