#pragma once
#include <LVUI/ColorSurface.h>
#include <algorithm>
#include <cstdint>
#include <list>
#include <optional>
#include <utility>
#include <vector>

namespace lvui::detail {
struct SurfaceRasterKey final {
    ColorSurfaceKind kind;
    std::uint32_t width,height;
    double hue=0,saturation=0,brightness=0;
    friend bool operator==(const SurfaceRasterKey&,const SurfaceRasterKey&)=default;
};

// These dependencies mirror iiColorManager::Picker::surfaceColor. A marker
// or alpha edit alone never changes any of these surface rasters.
inline SurfaceRasterKey surface_raster_key(ColorSurfaceKind kind,std::uint32_t width,std::uint32_t height,
                                           double hue,double saturation,double brightness) noexcept {
    SurfaceRasterKey result{kind,width,height};
    switch(kind){
    case ColorSurfaceKind::saturation_brightness:result.hue=hue;break;
    case ColorSurfaceKind::hue_saturation:result.brightness=brightness;break;
    case ColorSurfaceKind::brightness_rail:result.hue=hue;result.saturation=saturation;break;
    case ColorSurfaceKind::wheel:case ColorSurfaceKind::spectrum:case ColorSurfaceKind::hue_rail:break;
    }
    return result;
}

// Owned by the adapter under its existing mutex. Returned bytes remain the
// caller's independent value; cached data is bounded and least-recently used.
class SurfaceRasterCache final {
public:
    explicit SurfaceRasterCache(std::size_t budget=8*1024*1024,std::size_t limit=12)
        :budget_(budget),limit_(limit){}
    template<class Produce>
    std::optional<std::vector<std::uint8_t>> get(const SurfaceRasterKey& key,Produce&& produce){
        const auto found=std::find_if(entries_.begin(),entries_.end(),[&](const auto& entry){return entry.key==key;});
        if(found!=entries_.end()){
            entries_.splice(entries_.begin(),entries_,found);
            return entries_.front().rgba;
        }
        auto result=std::forward<Produce>(produce)();if(!result)return {};
        const auto size=result->size();
        if(!limit_||size>budget_||std::uint64_t(key.width)*key.height*4!=size)return result;
        while(!entries_.empty()&&(entries_.size()>=limit_||size>budget_-bytes_)){
            bytes_-=entries_.back().rgba.size();entries_.pop_back();
        }
        entries_.push_front({key,*result});bytes_+=size;
        return result;
    }
private:
    struct Entry final {SurfaceRasterKey key;std::vector<std::uint8_t> rgba;};
    std::list<Entry> entries_;
    std::size_t budget_,limit_,bytes_=0;
};
} // namespace lvui::detail
