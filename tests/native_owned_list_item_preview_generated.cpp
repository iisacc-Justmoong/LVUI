#include "NativeOwnedListItemPreview.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix,std::string_view text={}){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix)&&(text.empty()||value.text==text);});
    if(node==nodes.end())throw std::runtime_error("Generated preview control was not mounted: "+std::string(suffix)+" / "+std::string(text));
    return take(tree->find(node->id));
}
bool contains_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    for(const auto& run:frame.images)if(run.image==image)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.images)if(run.image==image)return true;return false;
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemPreview;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,220}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto frame=take(tree->drawing_snapshot({500,220}));require(contains_image(frame,scope->backend_0.preview.get()),"Generated resource lost its preview");
    auto more=find(tree,"preview.header-more");good(more.activate());good(find(tree,"/option-0","Duplicate").activate());
    require(scope->backend_0.events==std::vector<std::string>({"values:menu","event:menu","0:Duplicate","event:menuItem:0:Duplicate"}),
            "Generated resource menu lost its typed unified event");
    scope->backend_0.kind.set(lvui::ListItemVariant::media);(void)take(tree->drawing_snapshot({500,220}));
    (void)find(tree,"preview.actions.slot0","Stop");(void)find(tree,"preview.actions.slot1","Play");
    auto edited=find(tree,"/segment-1","Edited");good(edited.activate());
    require(scope->backend_0.version.get()==1&&scope->backend_0.events.back()=="version:1","Generated media segments did not update the model");
    scope->backend_0.show_preview.set(false);frame=take(tree->drawing_snapshot({500,220}));
    require(!contains_image(frame,scope->backend_0.preview.get()),"Reactive preview visibility was ignored");
    scope->backend_0.kind.set(lvui::ListItemVariant::resource);(void)take(tree->drawing_snapshot({500,220}));
    (void)find(tree,"preview.actions.slot0","Reveal");(void)find(tree,"preview.actions.slot1","Open");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated preview item retained its owner");tree->close();
}
