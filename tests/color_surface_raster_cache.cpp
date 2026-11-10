#include "../example/Integrations/App/ColorSurfaceRasterCache.h"
#include <LVUI/ColorSelectionSurface.h>
#include <iiColorManager/ColorManager.h>
#include <array>
#include <stdexcept>

namespace {
using namespace lvui;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ColorSurfaceResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
}
int main(){
    using namespace lvui::detail;
    const auto key=[](ColorSurfaceKind kind,double h,double s,double b){return surface_raster_key(kind,4,4,h,s,b);};
    const auto base=[](ColorSurfaceKind kind){return surface_raster_key(kind,4,4,10,20,30);};
    for(auto kind:{ColorSurfaceKind::wheel,ColorSurfaceKind::spectrum,ColorSurfaceKind::hue_rail})
        check(key(kind,90,70,60)==base(kind),"Invariant surface depended on selection state");
    check(key(ColorSurfaceKind::saturation_brightness,10,90,99)==base(ColorSurfaceKind::saturation_brightness)&&
          key(ColorSurfaceKind::saturation_brightness,11,20,30)!=base(ColorSurfaceKind::saturation_brightness),
          "Saturation-brightness raster has wrong hue dependency");
    check(key(ColorSurfaceKind::hue_saturation,90,70,30)==base(ColorSurfaceKind::hue_saturation)&&
          key(ColorSurfaceKind::hue_saturation,10,20,31)!=base(ColorSurfaceKind::hue_saturation),
          "Hue-saturation raster has wrong brightness dependency");
    check(key(ColorSurfaceKind::brightness_rail,10,20,99)==base(ColorSurfaceKind::brightness_rail)&&
          key(ColorSurfaceKind::brightness_rail,11,20,30)!=base(ColorSurfaceKind::brightness_rail)&&
          key(ColorSurfaceKind::brightness_rail,10,21,30)!=base(ColorSurfaceKind::brightness_rail),
          "Brightness raster has wrong hue or saturation dependency");

    SurfaceRasterCache cache(128,2);unsigned calls=0;
    const auto produce=[&]()->std::optional<std::vector<std::uint8_t>>{++calls;return std::vector<std::uint8_t>(64,std::uint8_t(calls));};
    const auto a=base(ColorSurfaceKind::wheel),b=base(ColorSurfaceKind::hue_rail),c=base(ColorSurfaceKind::spectrum);
    check(cache.get(a,produce)->front()==1&&cache.get(a,produce)->front()==1&&calls==1,"Same raster was recomputed");
    (void)cache.get(b,produce);(void)cache.get(a,produce);(void)cache.get(c,produce);
    (void)cache.get(b,produce);check(calls==4,"Bounded least-recently-used eviction failed");
    const auto failure_key=key(ColorSurfaceKind::brightness_rail,5,5,5);
    auto failed=cache.get(failure_key,[]()->std::optional<std::vector<std::uint8_t>>{return {};});
    check(!failed,"Failed raster generation was cached as a success");
    const auto before_retry=calls;(void)cache.get(failure_key,produce);
    check(calls==before_retry+1,"A failed raster did not retry");
    SurfaceRasterCache tiny(32,2);const auto before_oversize=calls;
    (void)tiny.get(a,produce);(void)tiny.get(a,produce);
    check(calls==before_oversize+2,"Raster larger than the budget was retained");

    auto selection=std::make_shared<ColorSelection>(StyleColor{.9,.2,.1,.5});
    ColorSelectionSurface source(selection);
    const std::array cases{
        std::pair{ColorSurfaceKind::wheel,iiColorManager::Surface::ColorWheel},
        std::pair{ColorSurfaceKind::hue_saturation,iiColorManager::Surface::BrightnessCube},
        std::pair{ColorSurfaceKind::saturation_brightness,iiColorManager::Surface::HueCube},
        std::pair{ColorSurfaceKind::spectrum,iiColorManager::Surface::Spectrum},
        std::pair{ColorSurfaceKind::hue_rail,iiColorManager::Surface::HueRail},
        std::pair{ColorSurfaceKind::brightness_rail,iiColorManager::Surface::BrightnessRail}};
    for(auto color:{StyleColor{.9,.2,.1,.5},StyleColor{.2,.7,.4,1},StyleColor{.9,.2,.1,.2},StyleColor{.3,.4,.9,.7}}){
        check(bool(selection->set_color(color)),"Color update failed");
        iiColorManager::Picker picker;check(picker.editColor({color.red,color.green,color.blue,color.alpha}),"Reference picker rejected color");
        for(auto [kind,surface]:cases){
            const auto actual=take(source.frame(kind,31,23));const auto reference=iiColorManager::renderSurface(picker,surface,31,23);
            check(reference&&actual.rgba==reference->rgba&&actual.revision==selection->revision(),
                  "Cached raster differs from the color engine's complete output");
        }
    }
}
