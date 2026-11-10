#include "NativeOwnedAvatar.generated.h"
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/SvgImageCache.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){
    if(!value)throw std::runtime_error(value.error().message);return std::move(*value);
}
}

int main(){
    auto source=lvui_generated::NativeOwnedAvatar::avatar_placeholder_source();
    require(bool(source),"Generated Avatar has no placeholder source");
    const auto high_pixels=take(ii::gui::avatar_placeholder_pixels({80,50},3));
    auto high=take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("avatar-placeholder",high_pixels));
    require(high&&high->width()==75&&high->height()==75,
        "Avatar placeholder did not use the allocated circle and backing scale");
    require(take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("avatar-placeholder",high_pixels))==high,
        "Unchanged Avatar placeholder input did not reuse the shared raster");
    auto resized=take((*source)({80,80},1));
    require(resized&&resized->width()==80&&resized->height()==80&&resized!=high,
        "Avatar placeholder did not follow resized bounds and minimum sampling");
    require(!take((*source)({0,50},2)),
        "An empty Avatar extent unexpectedly produced a raster");
    auto bounded=take((*source)({8192,8192},10));
    require(bounded&&bounded->width()==512&&bounded->height()==512,
        "Avatar placeholder exceeded the shared icon raster limit");
    require(!(*source)({40,40},0)&&
            !(*source)({-1,40},2)&&
            !(*source)({40,40},std::numeric_limits<double>::quiet_NaN()),
        "Avatar placeholder accepted an invalid size or scale");
    auto mask=take((*source)({80,50},3));
    require(mask&&mask->width()==75&&mask->height()==75&&
            take((*source)({80,50},3))==mask,
        "Avatar mask did not preserve its sampled size and cache identity");
    bool opaque=false;
    for(std::size_t i=3;i<mask->rgba().size();i+=4){
        opaque|=mask->rgba()[i]==255;
        require(std::abs(int(high->rgba()[i])-int(std::lround(mask->rgba()[i]*.8)))<=2,
            "Avatar mask changed the original silhouette");
    }
    require(opaque,"Avatar mask retained the captured color's 0.8 alpha");
    require(!take((*source)({0,50},2))&&
            !(*source)({40,40},0),
        "Avatar mask accepted an invalid request or allocated for an empty extent");
}
