#include <LVUI/NativeIconCache.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
struct Churn final:lvui::IconSource {
    mutable std::uint64_t version=0;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{
        ++version;return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg xmlns='http://www.w3.org/2000/svg'/>"});
    }
    std::uint64_t revision()const noexcept override{return version;}
};
}
int main(){
    lvui::native::NativeIconCache cache;
    auto builtin=take(cache.resolve("generaladd",{},18));require(builtin->width()==36&&builtin->height()==36,
        "Built-in icon raster extent is incorrect");
    bool visible=false;auto bytes=builtin->rgba();for(std::size_t i=3;i<bytes.size();i+=4)visible|=bytes[i]!=0;
    require(visible,"Built-in icon raster is empty");
    auto again=take(cache.resolve("generaladd",{},18));require(builtin==again,"Identical icon request missed its cache");
    for(auto name:{"procedure","generalchevronRight","generalchevronLeft"}){
        auto icon=take(cache.resolve(name,{},18));bool ink=false;auto rgba=icon->rgba();
        for(std::size_t i=3;i<rgba.size();i+=4)ink|=rgba[i]!=0;
        require(ink,"A built-in menu icon raster is empty");
    }
    auto larger=take(cache.resolve("generaladd",{},24));require(larger!=builtin&&larger->width()==48,"Icon size did not invalidate its cache");
    auto catalog=lvui::IconCatalog::create({{"ok","<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10'/></svg>"},
                                             {"bad","<svg xmlns='http://www.w3.org/2000/svg'><script>bad</script></svg>"}});
    require(bool(catalog),"Custom icon catalog was rejected");
    auto source=std::static_pointer_cast<const lvui::IconSource>(*catalog);
    auto custom=take(cache.resolve("ok",source,16));require(take(cache.resolve("ok",source,16))==custom,"Custom icon missed its cache");
    require(!cache.resolve("bad",source,16)&&take(cache.resolve("ok",source,16))==custom,
        "Invalid SVG damaged the previously cached icon");
    auto missing=cache.resolve("missing",source,16);require(!missing&&missing.error().code==ii::core::ErrorCode::not_found,
        "Unknown icon was not reported");
    require(!cache.resolve("ok",source,0)&&!cache.resolve("",source,16),"Invalid icon request was accepted");
    auto churn=std::make_shared<Churn>();auto unstable=cache.resolve("x",churn,16);
    require(!unstable&&unstable.error().code==ii::core::ErrorCode::busy&&churn->version==4,
        "An unstable source did not stop after bounded retries");
    bool rejected=false;std::thread worker([&]{auto result=cache.resolve("generaladd",{},18);
        rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"An icon cache was used from a foreign thread");
}
