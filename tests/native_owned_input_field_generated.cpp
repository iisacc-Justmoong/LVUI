#include "NativeOwnedInputField.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
std::string text(ii::gui::Widget field){auto snapshot=take(field.snapshot());return take(field.text_content({0,snapshot.text_length})).text;}
}
int main(){
    using View=lvui_generated::NativeOwnedInputField;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto graph=scope->native_lifetime.lock();
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/field");});
    require(found!=nodes.end(),"Generated input field is missing");auto field=take(tree->find(found->id));auto initial=take(field.snapshot());
    require(initial.text_input&&initial.kind==WidgetKind::absolute_stack&&text(field)=="Draft","Generated field is not LVUI-owned");
    auto clear_node=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/field.clear");});
    require(clear_node!=nodes.end(),"Generated input clear action is missing");auto clear=take(tree->find(clear_node->id));
    require(!take(clear.snapshot()).visible,"Generated clear action ignored its initial setting");
    scope->backend_0.clear.set(true);scope->backend_0.clearName.set("Reset draft");
    require(take(clear.snapshot()).visible&&take(clear.snapshot()).accessibility_name=="Reset draft"&&text(field)=="Draft",
            "Reactive clear options replaced text or lost accessibility");
    good(field.focus());good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(text(field)=="Draft한"&&scope->backend_0.text.get()=="Draft한"&&scope->backend_0.edits.size()==1,
            "Generated field lost its edit callback or reactive reentry");
    scope->backend_0.size.set(1);
    require(take(field.snapshot()).size.height==22&&text(field)=="Draft한",
            "Reactive compact size lost geometry or edited text");
    good(tree->input({.kind=WidgetInputKind::composition,.text="글"}));
    scope->backend_0.clear.set(false);scope->backend_0.clear.set(true);
    require(take(clear.snapshot()).visible&&take(field.snapshot()).marked_range.has_value(),
            "Reactive clear visibility interrupted marked text");
    scope->backend_0.surface.set(1);
    require(take(field.snapshot()).marked_range.has_value(),"Reactive rounded surface interrupted marked text");
    scope->backend_0.surface.set(2);
    require(take(graph->failures()).empty()&&take(field.snapshot()).marked_range.has_value(),
            "Reactive cylinder surface interrupted marked text");
    scope->backend_0.surface.set(3);
    require(take(graph->failures()).empty()&&take(field.snapshot()).marked_range.has_value(),
            "Reactive inline surface interrupted marked text");
    scope->backend_0.surface.set(9);
    require(!take(graph->failures()).empty()&&take(field.snapshot()).marked_range.has_value(),
            "Invalid reactive surface replaced marked text");
    scope->backend_0.surface.set(0);
    require(take(graph->failures()).empty()&&take(field.snapshot()).marked_range.has_value(),
            "Reactive surface did not recover without losing marked text");
    scope->backend_0.size.set(0);
    require(take(field.snapshot()).size.height==30&&text(field)=="Draft한글",
            "Changing the input size discarded the active composition");
    good(tree->input({.kind=WidgetInputKind::composition,.text=""}));
    scope->backend_0.size.set(2);
    require(!take(graph->failures()).empty()&&take(field.snapshot()).size.height==30&&text(field)=="Draft한",
            "Invalid input size replaced the field");
    scope->backend_0.size.set(1);require(take(graph->failures()).empty(),"Input size did not recover");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(text(field)=="Draft","Input size changes lost the previous undo entry");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(text(field)=="Draft한","Input size changes lost the redo entry");
    const auto edits_before_limit=scope->backend_0.edits.size();
    scope->backend_0.width.set(200);scope->backend_0.placeholder.set("Other");
    require(text(field)=="Draft한"&&take(field.snapshot()).size.width==200,"Reactive options replaced the field or its edit");
    scope->backend_0.maxLength.set(3);
    require(text(field)=="Dra"&&scope->backend_0.edits.size()==edits_before_limit,"Reactive length reduction did not trim without an edit callback");
    scope->backend_0.maxLength.set(-1);
    require(!take(graph->failures()).empty()&&text(field)=="Dra","Invalid reactive length changed the field");
    scope->backend_0.maxLength.set(3.5);
    require(!take(graph->failures()).empty()&&text(field)=="Dra","Fractional reactive length changed the field");
    scope->backend_0.maxLength.set(64);
    require(take(graph->failures()).empty()&&text(field)=="Dra","Reactive length did not recover");
    scope->backend_0.text.set("External");require(text(field)=="External","Reactive field text did not update");
    scope->backend_0.width.set(-1);scope->backend_0.text.set("Later");
    require(!take(graph->failures()).empty()&&text(field)=="External","Invalid options replaced the retained field");
    scope->backend_0.width.set(180);require(take(graph->failures()).empty()&&text(field)=="Later","Field did not recover after invalid options");
    scope->backend_0.password.set(true);require(take(field.snapshot()).password,"Reactive secure state did not reach the host");
    scope->backend_0.readOnly.set(true);good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    require(text(field)=="Later","Read-only generated field accepted input");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));require(scope->backend_0.submissions==1,"Generated field lost submission");
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::text,.text="y"}));
    require(text(field)=="Later","Disabled generated field accepted input");
    nodes=take(tree->snapshots());found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/multi");});
    require(found!=nodes.end(),"Generated multiline field is missing");auto multi=take(tree->find(found->id));good(multi.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::end}));
    const auto wrapped_end=take(multi.snapshot()).text_selection.location;
    require(wrapped_end>0&&wrapped_end<text(multi).size(),"Generated multiline field did not wrap by default");
    scope->backend_0.wrap.set(false);good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::end}));
    require(take(multi.snapshot()).text_selection.location==text(multi).size(),"Reactive wrapEnabled did not disable wrapping");
    scope->backend_0.wrap.set(true);
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::home}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::home,.control=true}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::end}));
    require(take(multi.snapshot()).text_selection.location==wrapped_end&&text(multi)=="one two three four",
            "Reactive wrapping did not recover without replacing text");
    good(host.clear_content());scope.reset();graph.reset();require(!field.snapshot(),"Retired generated field remained active");tree->close();
}
