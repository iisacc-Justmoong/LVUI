#include "NativeOwnedListDrag.generated.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using View=lvui_generated::NativeOwnedListDrag;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{220,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({220,100}));
    const auto send=[&](WidgetInputKind kind,double y){good(tree->input({.kind=kind,.x=10,.y=y,.buttons=1}));};
    send(WidgetInputKind::pointer_down,10);send(WidgetInputKind::pointer_move,58);send(WidgetInputKind::pointer_up,58);
    const auto snapshot=take(scope->backend_0.source->snapshot());
    require(snapshot->revision==1&&snapshot->entries[2].key=="a"&&scope->backend_0.moved_count==1&&
        scope->backend_0.key=="a"&&scope->backend_0.from==0&&scope->backend_0.to==2&&scope->backend_0.before.empty()&&
        scope->backend_0.revision.get()==1,"Generated list lost the move callback or reactive revision");
    scope->backend_0.reorderable.set(false);
    send(WidgetInputKind::pointer_down,10);send(WidgetInputKind::pointer_move,58);send(WidgetInputKind::pointer_up,58);
    require(take(scope->backend_0.source->snapshot())->revision==1&&scope->backend_0.moved_count==1,
        "A disabled generated list still moved its source");
    good(host.clear_content());tree->close();
}
