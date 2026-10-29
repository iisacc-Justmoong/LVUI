#include <LVUI/NativeAppHeader.h>
#include "NativeOwnedAppHeader.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void retained_actions(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,180}));lvui::AppHeaderConfiguration config;
    config.title="Project";config.subtitle="Editor";config.menu_visible=true;int menus=0;
    std::optional<lvui::native::NativeAppHeader> header;ii::gui::Widget action;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeAppHeader::create(builder,{},"header",config,[&]{++menus;});
        if(!created)return std::unexpected(created.error());header=*created;
        auto child=builder.action_surface(header->actions_root(),"action",{.text="Export",.minimum_height=28},{});
        if(!child)return std::unexpected(child.error());action=*child;return {};
    }));
    good(header->menu().activate());require(menus==1&&take(action.snapshot()).text=="Export","Header menu or action slot failed");
    good(header->menu().focus());auto focused=take(tree->drawing_snapshot({640,180}));const auto bounds=take(header->menu().snapshot()).bounds;
    const auto focus_ring=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){return panel.bounds==bounds&&panel.stroke_width>0;});};
    require(focus_ring(focused.panels)||std::ranges::any_of(focused.overlays,[&](const auto& layer){return focus_ring(layer.panels);}),
        "Focused menu lost its visible focus ring");
    int replaced=0;good(header->set_menu_clicked([&]{++replaced;}));good(header->menu().activate());
    require(replaced==1&&menus==1,"Replacing the menu callback changed its action slot");
    config.compact=true;config.title="Updated";config.subtitle="";config.action_spacing=12;good(header->set_options(config));
    require(take(action.snapshot()).text=="Export"&&take(header->root().measure(640)).height>=56,"Header updates replaced its action or broke minimum height");
    auto frame=take(tree->drawing_snapshot({640,180}));const auto contains=[](const auto& runs){return std::ranges::any_of(runs,[](const auto& item){return item.text=="Updated";});};
    require(contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);}),"Header title did not update");
    auto invalid=config;invalid.row_spacing=-1;require(!header->set_options(invalid)&&action.snapshot(),"Invalid header input changed the live action");
    good(outer.clear_content());require(!action.snapshot()&&!header->set_options(config),"Retired header remained usable");tree->close();
}
void generated(){
    using View=lvui_generated::NativeOwnedAppHeader;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,180}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    const auto find=[&](std::string_view suffix){auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
        if(found==nodes.end())throw std::runtime_error("Generated header widget missing");return take(tree->find(found->id));};
    auto menu=find("/header/@menu"),action=find("/action");good(menu.activate());
    require(scope->backend_0.menu_count==1&&take(action.snapshot()).text=="Action","Generated header menu or action failed");
    scope->backend_0.compact.set(true);scope->backend_0.title.set("Updated title");scope->backend_0.subtitle.set("");
    auto graph=scope->native_lifetime.lock();require(graph&&take(graph->failures()).empty()&&action.snapshot(),"Reactive header update lost its action");
    auto frame=take(tree->drawing_snapshot({640,180}));const auto contains=[](const auto& runs){return std::ranges::any_of(runs,[](const auto& item){return item.text=="Updated title";});};
    require(contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);}),"Generated header title did not update");
    std::weak_ptr<View::Scope> weak=scope;good(outer.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!action.snapshot()&&!menu.snapshot(),"Retired header retained its generated scope");tree->close();
}
void reentry_and_exception(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,180}));lvui::AppHeaderConfiguration config;config.title="Original";
    std::optional<lvui::native::NativeAppHeader> header;ii::gui::Widget action;bool throw_cancel=false,reentered=false;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeAppHeader::create(builder,{},"header",config);if(!created)return std::unexpected(created.error());header=*created;
        auto child=builder.action_surface(header->actions_root(),"action",{.text="Action",.minimum_width=100,.minimum_height=32},{});
        if(!child)return std::unexpected(child.error());action=*child;
        return child->set_control_events({.canceled=[&]{if(throw_cancel)throw std::runtime_error("Cancellation callback failed");
            auto latest=config;latest.title="Reentrant";good(header->set_options(latest));reentered=true;}});
    }));
    const auto press=[&]{auto bounds=take(action.snapshot()).bounds;good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+4}));};
    auto disabled=config;disabled.enabled=false;press();good(header->set_options(disabled));
    require(reentered&&take(header->root().snapshot()).enabled,"A reentrant header update was overwritten");
    throw_cancel=true;press();bool escaped=false;ii::core::Result<void> failed;
    try{failed=header->set_options(disabled);}catch(...){escaped=true;}
    require(!escaped&&!failed&&failed.error().cause,"A cancellation exception escaped the header interface");
    good(header->set_options(config));require(take(header->root().snapshot()).enabled,"Header did not recover after a callback exception");
    good(outer.clear_content());tree->close();
}
}
int main(){retained_actions();generated();reentry_and_exception();}
