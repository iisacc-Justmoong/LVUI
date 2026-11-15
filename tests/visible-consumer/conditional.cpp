#include "NativeVisibleConditional.generated.h"
#include "WidgetState.h"
#include <memory>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
int main(){
    using View=lvui_generated::NativeVisibleConditional;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    std::shared_ptr<View::Scope> scope;
    auto mounted=state->construct([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.content_host({},"host",{640,480});if(!root)return std::unexpected(root.error());
        return root->set_content([&](auto& child){return View::build(child,{}, {},&scope);});
    });
    if(!mounted)throw std::runtime_error(mounted.error().message);
    auto activated=state->activate_paint_view();if(!activated)throw std::runtime_error(activated.error().message);
    (void)take(state->drawing_snapshot({640,480}));
    if(!scope||scope->backend_0.visible_events.empty()||scope->backend_0.visible_events.back().empty()){
        std::string details;
        for(const auto& node:take(state->snapshots()))details+=node.id+":"+std::to_string(node.bounds.x)+","+std::to_string(node.bounds.y)+" "+std::to_string(node.bounds.width)+"x"+std::to_string(node.bounds.height)+" offset="+std::to_string(node.scroll_offset)+";";
        if(scope){details+=" events="+std::to_string(scope->backend_0.visible_events.size());for(auto& event:scope->backend_0.visible_events)details+="/"+std::to_string(event.size());}
        if(scope){auto failures=scope->binding_failures();if(failures)for(const auto& failure:*failures)details+=" error="+failure.failure.error.message;}
        throw std::runtime_error("Visible conditional did not publish its keys: "+details);
    }
    if(scope->backend_0.visible_events.back()!=std::vector<std::string>{"0","1","2"})throw std::runtime_error("Visible conditional published the wrong keys");
    scope->backend_0.show.set(false);
    if(!scope->backend_0.visible_events.back().empty())throw std::runtime_error("Retired conditional did not clear visible keys");
    scope->backend_0.show.set(true);
    (void)take(state->drawing_snapshot({640,480}));
    if(scope->backend_0.visible_events.back().empty())throw std::runtime_error("Restored conditional did not republish visible keys");
    state->close();
}
