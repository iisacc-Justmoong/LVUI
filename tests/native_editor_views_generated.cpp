#include "NativeEditorViews.generated.h"
#include <iiGUI/WidgetState.h>
#include <stdexcept>
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
int main(){using namespace ii::gui;using View=lvui_generated::NativeEditorViews;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,260}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](WidgetBuilder& b){return View::build(b,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({400,260}));auto canvas=scope->backend_0.canvas_source;auto timeline=scope->backend_0.timeline_source;
    check(canvas->view&&timeline->view&&canvas->queries==1&&timeline->queries==1,"LVUI did not mount the editor sources");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=20}));good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=20}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=212,.y=120}));good(tree->input({.kind=WidgetInputKind::pointer_up,.x=212,.y=120}));
    check(canvas->events==2&&timeline->events==2,"LVUI editor surfaces did not receive input");
    good(host.clear_content());check(!canvas->view->options()&&!timeline->view->options(),"editor sources survived their view generation");scope.reset();tree->close();
}
