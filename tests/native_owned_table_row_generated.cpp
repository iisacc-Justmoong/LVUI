#include "NativeOwnedTableRow.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view name){
    const auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.accessibility_name==name;});
    if(node==nodes.end())throw std::runtime_error("Generated row cell did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedTableRow;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({400,80}));auto cell=find(tree,"First");good(cell.activate());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    good(tree->input({.kind=WidgetInputKind::text,.text="Updated"}));good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(scope->backend_0.edits==std::vector<std::string>{"Updated"}&&scope->backend_0.submissions==std::vector<std::string>{"0:Updated"}&&
            scope->backend_0.cells.get()[0]=="Updated","Generated row lost its reactive cell edit");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(-1);
    require(graph&&!take(graph->failures()).empty(),"Invalid reactive row width was accepted");
    scope->backend_0.width.set(300);require(take(graph->failures()).empty(),"Row did not recover after invalid width");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!cell.snapshot(),"Generated row retained its owner");tree->close();
}
