#include "NativeOwnedScroll.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Generated scroll content was not found");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedScroll;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({240,160}));auto viewport=find(tree,"/viewport"),first=find(tree,"/first"),last=find(tree,"/last");
    auto before=take(viewport.snapshot());
    if(before.bounds.width!=180||before.bounds.height!=80||before.content_height<=80)
        throw std::runtime_error("Viewport geometry: width="+std::to_string(before.bounds.width)+" height="+
                                 std::to_string(before.bounds.height)+" content="+std::to_string(before.content_height));
    require(!take(last.snapshot()).visible&&take(last.snapshot()).clip_bounds==std::optional{before.bounds},
        "Offscreen content escaped the viewport");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=before.bounds.x+20,.y=before.bounds.y+20,.scroll_y=60}));
    require(take(viewport.snapshot()).scroll_offset>0,"Wheel input did not scroll the generated viewport");
    good(viewport.set_scroll_offset(10000));auto end=take(viewport.snapshot());auto button=take(last.snapshot());
    require(button.visible&&button.bounds.y+button.bounds.height<=end.bounds.y+end.bounds.height+.01,
        "The last control could not be revealed by scrolling");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=button.bounds.x+button.bounds.width/2,.y=button.bounds.y+button.bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=button.bounds.x+button.bounds.width/2,.y=button.bounds.y+button.bounds.height/2}));
    require(scope->backend_0.activated==1,"Scrolled child lost its pointer action");
    auto graph=scope->native_lifetime.lock();scope->backend_0.height.set(-1);
    require(!take(graph->failures()).empty()&&take(viewport.snapshot()).bounds.height==80,
        "Invalid reactive extent replaced the last valid viewport");
    scope->backend_0.height.set(120);require(take(graph->failures()).empty()&&take(viewport.snapshot()).bounds.height==120,
        "Valid reactive viewport extent did not recover");
    scope->backend_0.width.set(160);require(take(viewport.snapshot()).bounds.width==160,
        "Reactive viewport width did not update");
    scope->backend_0.enabled.set(false);good(viewport.set_scroll_offset(0));auto b=take(viewport.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::scroll,.x=b.x+20,.y=b.y+20,.scroll_y=30}));
    require(take(viewport.snapshot()).scroll_offset==0,"Disabled scroll accepted wheel input");
    good(last.activate());require(scope->backend_0.activated==2,"Disabling scrolling disabled its child action");
    scope->backend_0.enabled.set(true);good(last.focus());require(take(last.snapshot()).visible,"Focus did not reveal scrolled content");
    good(host.clear_content());scope.reset();graph.reset();require(!viewport.snapshot()&&!last.snapshot(),"Retired viewport or child remained active");tree->close();
}
