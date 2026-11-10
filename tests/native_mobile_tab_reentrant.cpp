#include <LVUI/NativeMobileTab.h>
#include <iiGUI/NativeMobileTabView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
using View=lvui::native::MobileTabBinding<ii::gui::BasicNativeMobileTab,ii::gui::CachedSvgDecoder>;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Source final:lvui::IconSource {
    mutable bool fired=false;
    std::function<void()> callback;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{
        if(!fired){fired=true;if(callback)callback();}
        return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg xmlns='http://www.w3.org/2000/svg' width='2' height='2'><rect width='2' height='2'/></svg>"});
    }
    std::uint64_t revision()const noexcept override{return 0;}
};
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto matches=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& run){return run.text==text;});};
    return matches(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return matches(layer.texts);});
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{160,100}));std::optional<View> tab;
    lvui::native::MobileTabInput initial;initial.item={"home","Initial","home","Initial"};initial.motion_enabled=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto made=View::create(builder,{},"tab",initial);if(!made)return std::unexpected(made.error());tab=std::move(*made);return {};
    }));
    auto latest=initial;latest.item.text="Latest";auto source=std::make_shared<Source>();
    source->callback=[&]{good(tab->set_options(latest));};
    auto stale=initial;stale.item.text="Stale";stale.item.icon_name="custom";stale.icon_source=source;
    good(tab->set_options(stale));auto frame=take(tree->drawing_snapshot({160,100}));
    require(contains(frame,"Latest")&&!contains(frame,"Stale"),"An older icon lookup replaced a reentrant newer update");
    auto retiring_source=std::make_shared<Source>();retiring_source->callback=[&]{good(host.clear_content());};
    auto retired=latest;retired.item.icon_name="retire";retired.icon_source=retiring_source;
    auto result=tab->set_options(retired);
    require(!result&&result.error().code==ii::core::ErrorCode::closed&&!tab->root().snapshot(),
            "An icon callback published after its mobile tab retired");
    tree->close();
}
