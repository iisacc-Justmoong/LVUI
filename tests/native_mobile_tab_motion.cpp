#include <LVUI/NativeMobileTab.h>
#include <iiGUI/NativeMobileTabView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
using View=lvui::native::MobileTabBinding<ii::gui::BasicNativeMobileTab,ii::gui::CachedSvgDecoder>;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;using namespace std::chrono_literals;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);auto preferences=std::make_shared<MotionPreferences>();
    detail::WidgetFrameHooks hooks;hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{160,100}));std::optional<View> tab;
    lvui::native::MobileTabInput input;input.item={"home","Home","home","Home"};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto made=View::create(builder,{},"tab",input);if(!made)return std::unexpected(made.error());tab=std::move(*made);return {};
    }));
    const auto start=ii::runtime::Clock::now();(void)frames.step(start);const auto idle=frames.stats().callbacks;
    (void)frames.step(start+10ms);require(frames.stats().callbacks==idle,"Idle mobile tab requested frames");
    input.selected=true;good(tab->set_options(input));(void)frames.step(start+20ms);
    require(frames.stats().callbacks>idle,"Mobile-tab selection did not request an animation frame");
    (void)frames.step(start+1s);const auto settled=frames.stats().callbacks;(void)frames.step(start+2s);
    require(frames.stats().callbacks==settled,"Settled mobile tab kept requesting frames");
    good(preferences->set_reduced_motion(true));input.selected=false;good(tab->set_options(input));
    const auto reduced=frames.stats().callbacks;(void)frames.step(start+3s);
    require(frames.stats().callbacks==reduced,"Reduced motion scheduled an animation");
    good(preferences->set_reduced_motion(false));input.selected=true;good(tab->set_options(input));
    input.enabled=false;good(tab->set_options(input));
    const auto disabled=frames.stats().callbacks;(void)frames.step(start+4s);
    require(frames.stats().callbacks==disabled,"Disabled mobile tab retained a scheduled frame");
    good(host.clear_content());require(!tab->set_options(input),"Retired mobile tab accepted motion updates");tree->close();
}
