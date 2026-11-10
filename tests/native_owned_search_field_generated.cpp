#include "NativeOwnedSearchField.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    for(const auto& item:take(tree->snapshots()))if(item.id.ends_with(std::string("/")+std::string(suffix)))return take(tree->find(item.id));
    throw std::runtime_error("Generated search widget missing: "+std::string(suffix));
}
std::string text(ii::gui::Widget value){auto s=take(value.snapshot());return take(value.text_content({0,s.text_length})).text;}
}
int main(){
    using View=lvui_generated::NativeOwnedSearchField;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto input=find(tree,"search.editor"),clear=find(tree,"search.clear");
    require(text(input)=="Draft"&&take(clear.snapshot()).visible&&take(input.snapshot()).accessibility_name=="Search sizes",
            "Generated search lost its initial text or semantics");
    good(input.focus());good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(scope->backend_0.query.get()=="Draft한"&&scope->backend_0.edits.size()==1,
            "Generated search did not deliver and synchronize a user edit");
    good(tree->input({.kind=WidgetInputKind::composition,.text="글"}));
    scope->backend_0.placeholder.set("Another placeholder");
    scope->backend_0.surface.set(2);
    scope->backend_0.surface.set(3);
    require(text(input)=="Draft한글"&&scope->backend_0.query.get()=="Draft한",
            "Reactive search options discarded marked text");
    auto graph=scope->native_lifetime.lock();
    scope->backend_0.surface.set(0);
    require(!take(graph->failures()).empty()&&text(input)=="Draft한글",
            "Invalid search surface replaced the active composition");
    scope->backend_0.surface.set(1);
    require(take(graph->failures()).empty()&&text(input)=="Draft한글",
            "Search surface did not recover without losing composition");
    good(tree->input({.kind=WidgetInputKind::composition,.text=""}));
    good(clear.activate());
    require(scope->backend_0.query.get().empty()&&take(input.snapshot()).focused,
            "Generated clear did not update the model and retain focus");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(scope->backend_0.query.get()=="Draft한"&&text(input)=="Draft한",
            "Generated clear did not preserve undo");
    scope->backend_0.clearVisible.set(false);require(!take(clear.snapshot()).visible,"Reactive clear visibility did not update");
    scope->backend_0.readOnly.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    require(text(input)=="Draft한","Reactive read-only search accepted input");
    scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty()&&text(input)=="Draft한","Invalid reactive search options replaced the field");
    scope->backend_0.width.set(180);require(take(graph->failures()).empty(),"Search field did not recover from invalid options");
    good(host.clear_content());scope.reset();graph.reset();require(!input.snapshot(),"Retired generated search field remained live");tree->close();
}
