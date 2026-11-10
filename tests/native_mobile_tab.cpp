#include <LVUI/NativeMobileTab.h>
#include <iiGUI/NativeMobileTabView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
using View=lvui::native::MobileTabBinding<ii::gui::BasicNativeMobileTab,ii::gui::CachedSvgDecoder>;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool text(const ii::gui::CanvasFrame& frame,std::string_view wanted){
    const auto contains=[&](const auto& items){return std::ranges::any_of(items,[&](const auto& item){return item.text==wanted;});};
    return contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);});
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{160,100}));std::optional<View> tab;int activated=0;
    lvui::native::MobileTabInput input;input.item={"home","Home","home","Home"};
    input.item.badge="2";input.selected=true;input.motion_enabled=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto made=View::create(builder,{},"tab",input,[&]{++activated;});if(!made)return std::unexpected(made.error());tab=std::move(*made);return {};
    }));
    auto root=tab->root();auto frame=take(tree->drawing_snapshot({160,100}));
    require(text(frame,"Home")&&text(frame,"2")&&take(root.snapshot()).action_role==ActionRole::tab,
            "Mobile tab did not draw its label and badge or expose tab semantics");
    good(root.activate());require(activated==1,"Mobile-tab activation was lost");
    const auto prior_size=take(root.snapshot()).size;
    auto invalid=input;invalid.width=-1;
    require(!tab->set_options(invalid)&&take(root.snapshot()).size==prior_size,"Invalid options changed a mobile tab");
    invalid=input;invalid.icon_revision=-1;
    require(!tab->set_options(invalid),"Negative icon revision was accepted");
    input.show_label=false;input.item.badge="";input.item.badge_dot=true;input.enabled=false;
    good(tab->set_options(input));frame=take(tree->drawing_snapshot({160,100}));
    require(!text(frame,"Home")&&!text(frame,"2")&&!take(root.snapshot()).enabled,"Updated mobile-tab state was not applied");
    good(root.activate());require(activated==1,"A disabled mobile tab activated");
    good(host.clear_content());require(!tab->set_options(input),"Retired mobile tab accepted an update");tree->close();
}
