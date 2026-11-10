#include "NativeCardOpen.generated.h"
#include <iiGUI/WidgetState.h>
#include <stdexcept>
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
int main(){using namespace ii::gui;using View=lvui_generated::NativeCardOpen;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
    auto host=take(tree->content_host({},"host",{210,240}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));(void)take(tree->drawing_snapshot({210,240}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=1,.buttons=1,.native_click_count=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=40,.timestamp=1.1}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=1.2,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=40,.timestamp=1.3}));
    if(scope->backend_0.clicks!=2||scope->backend_0.opens!=1)throw std::runtime_error("Generated Card lost selection or open event");
    good(host.clear_content());scope.reset();tree->close();}
