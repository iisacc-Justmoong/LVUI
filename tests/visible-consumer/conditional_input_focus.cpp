#include "NativeConditionalInputFocus.generated.h"
#include "WidgetState.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

template<class T>T take(ii::core::Result<T> value){
    if(!value)throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    auto nodes=take(state->snapshots());
    auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Missing conditional focus target");
    return take(state->find(found->id));
}
int main(){
    using View=lvui_generated::NativeConditionalInputFocus;using namespace ii::gui;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    Widget host;std::shared_ptr<View::Scope> scope;
    good(state->construct([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto made=builder.content_host({},"host",{400,300});if(!made)return std::unexpected(made.error());host=*made;
        return host.set_content([&](auto& nested){return View::build(nested,{}, {},&scope);});
    }));
    good(state->activate_paint_view());(void)take(state->drawing_snapshot({400,300}));
    auto first=find(state,"/first");
    const auto enter=[&]{good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));};
    check(take(first.snapshot()).focused,"Initial field was not focused");
    enter();check(scope->backend_0.submissions==1&&take(first.snapshot()).focused,"Inactive target changed focus or suppressed submission");
    scope->backend_0.show.set(true);scope->backend_0.inner.set(true);
    auto next=find(state,"/next");
    enter();check(take(next.snapshot()).focused,"Nested active target did not receive focus");
    good(first.focus());scope->backend_0.enabled.set(false);enter();
    check(take(first.snapshot()).focused,"Disabled conditional target stole focus");
    scope->backend_0.enabled.set(true);scope->backend_0.visible.set(false);enter();
    check(take(first.snapshot()).focused,"Hidden conditional target stole focus");
    scope->backend_0.visible.set(true);scope->backend_0.inner.set(false);scope->backend_0.inner.set(true);
    enter();next=find(state,"/next");check(take(next.snapshot()).focused,"Remounted target was not resolved on a later submission");
    good(first.focus());scope->backend_0.retire_on_submit=true;enter();
    check(!next.focus(),"Retired conditional target accepted focus");
    scope->backend_0.show.set(true);
    auto replacement=find(state,"/next");
    check(!take(replacement.snapshot()).focused,"Submission focused a replacement created by its own callback");
    enter();check(take(replacement.snapshot()).focused,"Replacement was not available to the next submission");
    Widget conditionalHost;
    for(const auto& node:take(state->snapshots())){
        auto candidate=state->find(node.id);if(!candidate)continue;
        if(candidate->content_item("next")){conditionalHost=*candidate;break;}
    }
    check(!conditionalHost.is_null(),"Could not locate the conditional activation host");
    good(first.focus());
    scope->backend_0.during_submit=[conditionalHost]{
        good(conditionalHost.set_content([](WidgetBuilder& builder)->ii::core::Result<void>{
            auto next=builder.input_field({},"next");if(!next)return std::unexpected(next.error());return {};
        }));
    };
    enter();check(!replacement.focus(),"Callback did not retire the original target");
    replacement=find(state,"/next");
    check(!take(replacement.snapshot()).focused,"A stale focus request reached the callback's replacement");
    enter();check(take(replacement.snapshot()).focused,"Callback's replacement was not available to the next submission");
    good(host.clear_content());scope.reset();state->close();
}
