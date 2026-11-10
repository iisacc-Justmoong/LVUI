#include "NativeOwnedTableViews.generated.h"
#include "NativeOwnedTableColumnViews.generated.h"
#include "NativeOwnedTableHeaderViews.generated.h"
#include "NativeOwnedTableActionViews.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
using View=lvui_generated::NativeOwnedTableViews;
using State=ii::gui::detail::WidgetState;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::TableResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view value){
    for(const auto& text:frame.texts)if(text.text==value)return true;
    for(const auto& layer:frame.overlays)for(const auto& text:layer.texts)if(text.text==value)return true;
    return false;
}
ii::gui::Widget value_widget(const std::shared_ptr<State>& tree,std::string_view value){
    for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/cell-value")&&item.text==value)return take(tree->find(item.id));
    throw std::runtime_error("Generated custom table cell not found");
}
}
int main(){
    auto tree=std::make_shared<State>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,120}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto first=value_widget(tree,"First");auto frame=take(tree->drawing_snapshot({240,120}));
    require(has_text(frame,"Header")&&has_text(frame,"Cell")&&has_text(frame,"First"),"Generated table delegates were not drawn");
    const auto revision=take(scope->backend_0.source->edit({.revision=0,.cells={{"r0","a","Renamed"}}}));
    scope->backend_0.revision.set(std::int64_t(revision));
    require(take(first.snapshot()).text=="Renamed"&&has_text(take(tree->drawing_snapshot({240,120})),"Renamed"),"Table context did not publish the new cell value");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=10,.y=34,.buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=10,.y=34,.buttons=1}));
    require(take(scope->backend_0.controller.get()->selection()).has_value(),"Noninteractive custom cells blocked table selection");
    bool selected=false;for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/cell-selected")&&item.text=="true")selected=true;
    require(selected,"A visible cell View did not receive the selection change");
    good(host.clear_content());require(!first.snapshot(),"Detached table retained its custom cell");
    std::shared_ptr<lvui_generated::NativeOwnedTableColumnViews::Scope> columns;
    good(host.set_content([&](auto& builder){return lvui_generated::NativeOwnedTableColumnViews::build(builder,{}, {},&columns);}));
    frame=take(tree->drawing_snapshot({240,120}));
    require(has_text(frame,"Alpha")&&has_text(frame,"First")&&has_text(frame,"Beta")&&has_text(frame,"Second"),
        "Column-specific Views did not compose with default columns");
    bool found_cell=false;for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/custom-cell")&&item.text=="First")found_cell=true;
    require(found_cell,"The configured cell View was not mounted");
    good(host.clear_content());
    good(host.set_content([&](auto& builder){return lvui_generated::NativeOwnedTableHeaderViews::build(builder,{}, {});}));
    frame=take(tree->drawing_snapshot({240,120}));
    require(has_text(frame,"Alpha")&&has_text(frame,"Beta"),"Standalone header did not combine custom and default columns");
    bool found_header=false;for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/custom-heading")&&item.text=="Alpha")found_header=true;
    require(found_header,"The standalone header View was not mounted");
    good(host.clear_content());
    std::shared_ptr<lvui_generated::NativeOwnedTableActionViews::Scope> actions;
    good(host.set_content([&](auto& builder){return lvui_generated::NativeOwnedTableActionViews::build(builder,{}, {},&actions);}));
    (void)take(tree->drawing_snapshot({240,120}));
    std::optional<ii::gui::Rect> button_bounds;
    for(const auto& item:take(tree->snapshots()))if(item.id.ends_with("/open-cell")&&item.text=="First"&&item.enabled)button_bounds=item.bounds;
    require(button_bounds&&button_bounds->width>0&&button_bounds->height>0,"The custom cell button has no input bounds");
    const auto x=button_bounds->x+button_bounds->width/2,y=button_bounds->y+button_bounds->height/2;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=x,.y=y,.buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=x,.y=y,.buttons=1}));
    require(actions->backend_0.opened=="r0","The cell button did not deliver its row key to the ViewModel");
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::space}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::space}));
    if(actions->backend_0.open_count!=2){std::string focused;for(const auto& item:take(tree->snapshots()))if(item.focused)focused+=item.id+" ";
        throw std::runtime_error("The cell button did not receive its keyboard activation; focused="+focused);}
    require(!take(actions->backend_0.controller.get()->selection()).has_value(),"The cell button also triggered the table selection");
    good(host.clear_content());tree->close();
}
