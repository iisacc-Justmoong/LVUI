#include <LVUI/NativeAuthoredIcons.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeAvatarAssets.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Decoder final {
    using Image=std::string;inline static unsigned calls=0;inline static std::uint32_t size=0;
    inline static bool empty=false;
    static ii::core::Result<std::shared_ptr<const Image>> decode(std::shared_ptr<const void> identity,std::string_view svg,std::uint32_t pixels){
        if(!identity)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Missing identity");
        ++calls;size=pixels;if(empty)return std::shared_ptr<const Image>{};return std::make_shared<const Image>(svg);
    }
};
struct Source final:lvui::IconSource {
    mutable std::uint64_t version=0;bool churn=false;
    std::shared_ptr<const lvui::IconAsset> asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg/>"});
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view name)const override{
        if(name!="x")return std::unexpected(lvui::IconError{lvui::IconErrorCode::not_found,"Missing asset"});
        if(churn)++version;return asset;
    }
    std::uint64_t revision()const noexcept override{return version;}
};
}
int main(){
    auto catalog=lvui::native::avatar_placeholder_mask_catalog();
    require(bool(catalog),"Avatar mask asset is unavailable without iiGUI");
    auto mask=(*catalog)->resolve("avatar-placeholder-mask");
    require(mask&&(*mask)->svg.find("fill-opacity=\"0.8\"")==std::string::npos&&
            (*mask)->svg.find("fill-opacity=\"1\"")!=std::string::npos,
            "Avatar mask asset did not retain its full-coverage paths");
    auto again=lvui::native::avatar_placeholder_mask_catalog();
    require(again&&*again==*catalog,"Avatar mask asset was recreated");
    auto source=std::make_shared<Source>();
    auto image=lvui::native::icon_image_for_logical_size<Decoder>(source,"x",12);
    require(image&&**image=="<svg/>"&&Decoder::calls==1&&Decoder::size==24,"Injected decoder did not receive the LVUI asset and requested raster size");
    require(!lvui::native::icon_image_for_logical_size<Decoder>(source," ",12)&&
            !lvui::native::icon_image_for_logical_size<Decoder>(source,"x",-1)&&
            !lvui::native::icon_image<Decoder>({},"x",24),"Invalid asset request was accepted");
    auto missing=lvui::native::icon_image<Decoder>(source,"unknown",24);
    require(!missing&&missing.error().code==ii::core::ErrorCode::not_found,"Missing asset did not preserve its error");
    source->churn=true;const auto before=source->version;
    auto unstable=lvui::native::icon_image<Decoder>(source,"x",24);
    require(!unstable&&unstable.error().code==ii::core::ErrorCode::busy&&source->version==before+4&&Decoder::calls==1,
            "Unstable source was not bounded before decoding");
    source->churn=false;std::string large="<svg>";large.append(70000,' ');large+="</svg>";
    source->asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{large});
    auto decoded=lvui::native::icon_image<Decoder>(source,"x",24);
    require(decoded&&**decoded==large&&Decoder::calls==2,"A valid SVG larger than 64 KiB was rejected");
    lvui::native::NativeIconCache<Decoder> large_cache;
    auto cached_large=large_cache.resolve("x",source,12);
    require(cached_large&&**cached_large==large,"A valid large SVG was rejected by the per-component cache");
    source->asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{std::string(1024*1024+1,'x')});++source->version;
    require(!large_cache.resolve("x",source,12),"An oversized SVG was accepted by the cache");
    source->asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{std::string("<svg>\0</svg>",12)});++source->version;
    require(!large_cache.resolve("x",source,12),"An SVG with a NUL byte was accepted by the cache");
    source->asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg/>"});++source->version;
    lvui::native::NativeIconCache<Decoder> cache;
    auto first=cache.resolve("x",source,12);
    const auto calls=Decoder::calls;
    auto repeated=cache.resolve("x",source,12);
    require(first&&repeated&&*first==*repeated&&Decoder::calls==calls,
            "The owner-thread cache decoded an unchanged asset again");
    auto wrong_thread=ii::core::ErrorCode::failed;
    std::thread worker([&]{auto result=cache.resolve("x",source,12);if(!result)wrong_thread=result.error().code;});
    worker.join();require(wrong_thread==ii::core::ErrorCode::wrong_executor,"Cache accepted another thread");
    source->asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg id='new'/>"});++source->version;
    auto changed=cache.resolve("x",source,12);
    require(changed&&**changed=="<svg id='new'/>"&&*changed!=*first&&Decoder::calls==calls+1,
            "Cache retained a superseded source revision");
    auto invalid=cache.resolve("missing",source,12);
    auto preserved=cache.resolve("x",source,12);
    require(!invalid&&preserved&&*preserved==*changed&&Decoder::calls==calls+1,
            "Failed lookup corrupted the cached asset");
    Decoder::empty=true;
    auto null_result=cache.resolve("x",source,13);
    Decoder::empty=false;
    preserved=cache.resolve("x",source,12);
    require(!null_result&&preserved&&*preserved==*changed,"An empty decoder result was cached");
    source->churn=true;const auto version=source->version;
    auto changing=cache.resolve("x",source,13);
    require(!changing&&changing.error().code==ii::core::ErrorCode::busy&&source->version==version+4,
            "Cache did not bound retries when the source changes");
}
