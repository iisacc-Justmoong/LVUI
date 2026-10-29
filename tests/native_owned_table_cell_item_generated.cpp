#include "NativeOwnedTableCellItem.generated.h"
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
    if(node==nodes.end())throw std::runtime_error("Generated table cell did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedTableCellItem;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,80}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto surface=find(tree,"/cell.surface"),root=find(tree,"/cell");good(surface.activate());
    const auto replace=[&](std::string text){good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
        good(tree->input({.kind=WidgetInputKind::text,.text=std::move(text)}));};
    replace("bad");good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(scope->backend_0.rejections==std::vector<std::string>{"bad"}&&scope->backend_0.edits.back()=="bad",
            "Generated cell did not report its invalid draft");
    replace("12");good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(scope->backend_0.submissions==std::vector<std::string>{"12"}&&scope->backend_0.text.get()=="12",
            "Generated cell did not submit the valid reactive value");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(-1);
    require(graph&&!take(graph->failures()).empty(),"Invalid reactive cell width was accepted");
    scope->backend_0.width.set(200);require(take(graph->failures()).empty(),"Cell did not recover after invalid width");
    scope->backend_0.visible.set(false);require(!take(root.interaction()).visible,"Reactive cell visibility was ignored");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!surface.snapshot(),"Generated cell retained its owner");tree->close();
}
