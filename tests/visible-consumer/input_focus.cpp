#include "NativeInputFocus.generated.h"
#include "WidgetState.h"
#include <memory>
#include <stdexcept>

template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
int main(){
    using View=lvui_generated::NativeInputFocus;using namespace ii::gui;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    Widget host;
    good(state->construct([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto made=builder.content_host({},"host",{400,300});if(!made)return std::unexpected(made.error());host=*made;
        return host.set_content([&](auto& nested){return View::build(nested,{});});
    }));
    good(state->activate_paint_view());(void)take(state->drawing_snapshot({400,300}));
    auto first=take(host.content_item("first")),next=take(host.content_item("next"));
    if(!take(first.snapshot()).focused)throw std::runtime_error("Published input did not take its requested initial focus");
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    if(!take(next.snapshot()).focused)throw std::runtime_error("Submit did not focus its target");
    good(first.focus());good(next.set_enabled(false));
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    if(!take(first.snapshot()).focused)throw std::runtime_error("Disabled submit target stole focus");
    good(host.clear_content());if(first.focus())throw std::runtime_error("Retired input accepted focus");
    state->close();
}
