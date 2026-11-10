#include "NativeTextEditorScroll.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

#if !defined(IIGUI_REVEAL_LOCAL_RECT_API)
#error This integration test requires a matching iiGUI header and library.
#endif
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& n){return n.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Missing editor scroll fixture");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeTextEditorScroll;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    scope->backend_0.text.set("line 1\nline 2\nline 3\nline 4\nline 5\nline 6\nline 7\nline 8\nline 9\nline 10");
    (void)take(tree->drawing_snapshot({240,160}));auto field=find(tree,"/editor"),viewport=find(tree,"/viewport");
    good(field.focus());good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{0,0}}));
    good(viewport.set_scroll_offset(0));
    const auto end=take(field.snapshot()).text_length;
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{end,0}}));
    check(take(viewport.snapshot()).scroll_offset>0,"Moving the focused editor caret did not reveal it in the ancestor");
    good(host.clear_content());scope.reset();check(!field.snapshot(),"Retired editor remained active");tree->close();
}
