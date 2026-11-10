#include "NativeEmbeddedCounter.generated.h"
#include "NativeEmbeddedParent.generated.h"
#include "NativeEmbeddedBranch.generated.h"
#include "NativeEmbeddedDual.generated.h"
#include "NativeEmbeddedBadge.generated.h"
#include "NativeEmbeddedRepeated.generated.h"
#include "NativeEmbeddedSequence.generated.h"
#include "NativeEmbeddedObjectRoot.generated.h"
#include "NativeEmbeddedBootstrap.generated.h"
#include "NativeEmbeddedShell.generated.h"
#include "NativeCompatibilityWindows.generated.h"
#include <iiGUI/CompiledViewSource.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Counter=lvui_generated::NativeEmbeddedCounter;
using Parent=lvui_generated::NativeEmbeddedParent;
using Branch=lvui_generated::NativeEmbeddedBranch;
using Dual=lvui_generated::NativeEmbeddedDual;
using Badge=lvui_generated::NativeEmbeddedBadge;
using Repeated=lvui_generated::NativeEmbeddedRepeated;
using Sequence=lvui_generated::NativeEmbeddedSequence;
using ObjectRoot=lvui_generated::NativeEmbeddedObjectRoot;
using Bootstrap=lvui_generated::NativeEmbeddedBootstrap;
using Shell=lvui_generated::NativeEmbeddedShell;
using Windows=lvui_generated::NativeCompatibilityWindows;
static_assert(Windows::scene_count==5&&Windows::scenes[0].initially_open&&
    !Windows::scenes[1].initially_open&&!Windows::scenes[2].initially_open&&
    Windows::scenes[3].initially_open&&Windows::scenes[4].initially_open);
using State=ii::gui::detail::WidgetState;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void good(ii::core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
template<class T>T take(ii::core::Result<T> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}return std::move(*result);}
ii::gui::Widget find(const std::shared_ptr<State>& tree,std::string_view name){
    for(const auto& node:take(tree->snapshots()))if(node.id.ends_with("/"+std::string(name)))return take(tree->find(node.id));
    throw std::runtime_error("Embedded widget was not found: "+std::string(name));
}
void native_attachment(){
    bool empty_rejected=false;try{(void)ii::gui::compiled_view_source(ii::gui::WidgetFactory{});}catch(const std::invalid_argument&){empty_rejected=true;}
    require(empty_rejected,"An empty compiled View factory was accepted");
    auto first=ii::gui::compiled_view_source<Counter>();
    auto second=ii::gui::compiled_view_source<Counter>();
    auto rejected=ii::gui::compiled_view_source([](ii::gui::WidgetBuilder&)->ii::core::Result<void>{
        return ii::core::fail(ii::core::ErrorCode::failed,"candidate rejected");
    });
    auto throwing=ii::gui::compiled_view_source([](ii::gui::WidgetBuilder&)->ii::core::Result<void>{throw std::runtime_error("factory exception");});
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,400}));
    std::shared_ptr<Parent::Scope> scope;
    good(host.set_content([&](auto& widgets){return Parent::build(widgets,{},[&](auto& state){state.backend_0.source.set(first);},&scope);}));
    auto value=find(tree,"value"),button=find(tree,"increment");
    require(take(value.snapshot()).text=="0","Embedded View did not initialize");
    good(button.activate());
    require(take(value.snapshot()).text=="1","Embedded View did not update its session");
    scope->backend_0.source.set(rejected);
    require(take(value.snapshot()).text=="1"&&!take(scope->binding_failures()).empty(),"Rejected candidate lost the current View");
    scope->backend_0.source.set(throwing);
    require(take(value.snapshot()).text=="1","A throwing candidate displaced the current View");
    scope->backend_0.source.set(first);
    require(take(value.snapshot()).text=="1","Restoring the same source reset its session");
    scope->backend_0.source.set(second);
    require(!value.snapshot()&&take(find(tree,"value").snapshot()).text=="0","Replacing a source did not retire and recreate the View");
    good(find(tree,"increment").activate());
    require(take(find(tree,"value").snapshot()).text=="1","Replacement View did not respond to input");
    auto old=find(tree,"increment");
    good(host.clear_content());scope.reset();
    require(!old.snapshot(),"Detached parent left an embedded View active");
    tree->close();
}
void independent_roots(){
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,400}));std::shared_ptr<Branch::Scope> branch_scope;
    auto branch=ii::gui::compiled_view_source([&](ii::gui::WidgetBuilder& widgets){return Branch::build(widgets,{}, {},&branch_scope);});
    good(host.set_content([&](auto& widgets){return branch->build(widgets);}));
    auto old=find(tree,"branch-off");require(take(old.snapshot()).text=="Hidden","Conditional root did not mount its first branch");
    branch_scope->backend_0.shown.set(true);
    require(!old.snapshot()&&take(find(tree,"branch-on").snapshot()).text=="Shown","Conditional root did not replace its branch");
    good(host.set_content([](auto& widgets){return Badge::build(widgets);}));
    require(take(find(tree,"badge").snapshot()).text=="Standalone","Parameterized root did not mount");
    good(host.set_content([](auto& widgets){return Repeated::build(widgets);}));
    unsigned labels=0;for(const auto& node:take(tree->snapshots()))if(node.text=="1"||node.text=="2"||node.text=="3")++labels;
    require(labels==3,"Repeated root did not mount all rows");
    good(host.set_content([](auto& widgets){return Sequence::build(widgets);}));
    require(take(find(tree,"one").snapshot()).text=="One"&&take(find(tree,"two").snapshot()).text=="Two"&&
        take(find(tree,"@lvui/0").stack_layout()).stack.spacing==0,"Root sequence added spacing or lost its Views");
    std::shared_ptr<ObjectRoot::Scope> object_scope;
    good(host.set_content([&](auto& widgets){return ObjectRoot::build(widgets,{}, {},&object_scope);}));
    auto title=find(tree,"root-title");require(take(title.snapshot()).text=="Record","Root View did not receive its C++ object");
    object_scope->backend_0.record.set({"Updated",5});
    require(take(title.snapshot()).text=="Updated","Root C++ object change did not reach the View");
    good(host.set_content([](auto& widgets){return Bootstrap::build(widgets);}));
    require(take(find(tree,"bootstrap").snapshot()).text=="Bootstrap","Bootstrap content did not mount");
    good(host.set_content([](auto& widgets){return Shell::build(widgets);}));
    require(take(find(tree,"shell").snapshot()).text=="Shell","Shell content did not mount");
    good(host.clear_content());tree->close();
}
void shared_source_sessions(){
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,400}));auto source=ii::gui::compiled_view_source<Counter>();
    good(host.set_content([&](auto& widgets){return Dual::build(widgets,{},[&](auto& scope){scope.backend_0.source.set(source);});}));
    std::vector<ii::gui::Widget> buttons,labels;
    for(const auto& node:take(tree->snapshots())){
        if(node.id.ends_with("/increment"))buttons.push_back(take(tree->find(node.id)));
        if(node.id.ends_with("/value"))labels.push_back(take(tree->find(node.id)));
    }
    require(buttons.size()==2&&labels.size()==2,"A shared source did not create two Views");
    good(buttons.front().activate());
    const auto first=take(labels[0].snapshot()).text,second=take(labels[1].snapshot()).text;
    require((first=="1"&&second=="0")||(first=="0"&&second=="1"),"Shared source activations shared mutable state");
    good(host.clear_content());require(!buttons.front().snapshot()&&!buttons.back().snapshot(),"Shared source views were not retired");tree->close();
}
}
int main(){native_attachment();independent_roots();shared_source_sessions();}
