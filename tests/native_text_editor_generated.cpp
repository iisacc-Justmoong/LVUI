#include "NativeTextEditor.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::string text(ii::gui::Widget field){auto s=take(field.snapshot());return take(field.text_content({0,s.text_length})).text;}
}
int main(){
    using View=lvui_generated::NativeTextEditor;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto graph=scope->native_lifetime.lock();
    auto nodes=take(tree->snapshots());auto it=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/notes");});
    check(it!=nodes.end(),"Generated TextEditor is missing");auto field=take(tree->find(it->id));
    auto initial=take(field.snapshot());
    if(!(initial.text_input&&initial.size.height==72&&text(field)=="Project"))
        throw std::runtime_error("Initial editor: input="+std::to_string(initial.text_input)+" height="+std::to_string(initial.size.height)+" text="+text(field));
    check(!scope->backend_0.content_heights.empty()&&scope->backend_0.content_heights.back()>0,"Initial text measurement was not delivered");
    good(field.focus());good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    check(text(field)=="Project한"&&scope->backend_0.text.get()=="Project한"&&scope->backend_0.edits.size()==1,
          "Committed text was not sent once or reactive reentry lost it");
    good(tree->input({.kind=WidgetInputKind::composition,.text="글"}));
    scope->backend_0.height.set(90);scope->backend_0.inset.set(6);
    check(take(field.snapshot()).marked_range.has_value()&&scope->backend_0.edits.size()==1&&take(field.snapshot()).size.height==90,
          "Geometry update interrupted IME composition");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    check(text(field)=="Project한글"&&scope->backend_0.edits.size()==2,"Composition was not committed exactly once");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    check(text(field)=="Project한","Undo failed after composition");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    check(text(field)=="Project한글","Redo failed after composition");
    const auto edit_count=scope->backend_0.edits.size();scope->backend_0.text.set("External\nnotes");
    check(text(field)=="External\nnotes"&&scope->backend_0.edits.size()==edit_count,"External change emitted an edit");
    const auto before_measure=scope->backend_0.content_heights.size();
    scope->backend_0.adaptive=true;
    scope->backend_0.text.set("A long prompt to wrap to several lines and change the content height as the user types words repeatedly across a substantial number of lines of text");
    check(scope->backend_0.content_heights.size()>before_measure,"A changed wrapped text height did not emit a measurement");
    (void)take(tree->drawing_snapshot({400,300}));
    check(take(field.snapshot()).size.height==scope->backend_0.height.get()&&scope->backend_0.height.get()<=160,
          "The measured content height did not resize the bound editor");
    scope->backend_0.adaptive=false;
    const auto after_measure=scope->backend_0.content_heights.size();
    scope->backend_0.inset.set(7);
    check(scope->backend_0.content_heights.size()==after_measure,"Unchanged content height emitted a duplicate measurement");
    const auto height_before_width=scope->backend_0.content_heights.back();scope->backend_0.width.set(80);
    (void)take(tree->drawing_snapshot({400,300}));
    check(scope->backend_0.content_heights.back()>height_before_width,"A width-only reflow did not update the content measurement");
    scope->backend_0.width.set(160);(void)take(tree->drawing_snapshot({400,300}));
    scope->backend_0.text.set("External\nnotes");
    scope->backend_0.minimumHeight.set(-1);scope->backend_0.text.set("Pending");
    check(!take(graph->failures()).empty()&&text(field)=="External\nnotes","Invalid configuration replaced editor state");
    scope->backend_0.minimumHeight.set(72);
    check(take(graph->failures()).empty()&&text(field)=="Pending","Editor did not recover from invalid model values");
    scope->backend_0.readOnly.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    check(text(field)=="Pending","Read-only editor accepted a user edit");
    good(host.clear_content());scope.reset();graph.reset();check(!field.snapshot(),"Retired editor remained active");tree->close();
}
