#include "NativeOwnedTableHeader.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated table header did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedTableHeader;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto header=find(tree,"/header");auto root=take(header.snapshot());
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(scope->backend_0.clicked==std::vector<std::string>{"Second"},"Generated header lost the clicked column");
    scope->backend_0.columns.set({"One","Two","Three"});
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(scope->backend_0.clicked.back()=="Two","Generated header did not update its columns");
    auto graph=scope->native_lifetime.lock();scope->backend_0.columns.set({"Only"});
    require(graph&&!take(graph->failures()).empty(),"Mismatched reactive column widths were accepted");
    scope->backend_0.columns.set({"One","Two","Three"});require(take(graph->failures()).empty(),"Header did not recover after invalid columns");
    scope->backend_0.visible.set(false);require(!take(header.interaction()).visible,"Reactive header visibility was ignored");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!header.snapshot(),"Generated header retained its owner");tree->close();
}
