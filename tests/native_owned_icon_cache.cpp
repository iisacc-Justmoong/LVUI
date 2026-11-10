namespace lvui::native::detail {}
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/SvgImageCache.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <iostream>
#include <vector>
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
void compare_reference(const std::shared_ptr<const ii::gui::RasterImage>& image,const char* filename){
    std::ifstream file(std::string(LVUI_ICON_FALLBACK_REFERENCE_DIR)+"/"+filename,std::ios::binary);
    require(bool(file),"Missing filtered icon reference image");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    auto reference=take(ii::gui::png_image(bytes));
    require(reference->width()>=image->width()&&reference->height()>=image->height(),"Reference extent differs");
    auto expected=reference->rgba(),actual=image->rgba();
    std::uint64_t difference=0, visible=0;
    for(std::size_t i=0;i<actual.size();i+=4){
        const auto pixel=i/4, x=pixel%image->width(), y=pixel/image->width();
        const auto ref=(y*reference->width()+x)*4;
        visible+=expected[ref+3]!=0;
        difference+=std::abs(int(expected[ref+3])-int(actual[i+3]));
        for(unsigned channel=0;channel<3;++channel){
            const auto left=(int(expected[ref+channel])*int(expected[ref+3])+127)/255;
            const auto right=(int(actual[i+channel])*int(actual[i+3])+127)/255;
            difference+=std::abs(left-right);
        }
    }
    const double mean=double(difference)/double(actual.size());
    std::cout<<"FILTERED_ICON_REFERENCE "<<filename<<" VISIBLE "<<visible<<" MEAN_PREMULTIPLIED_ERROR "<<mean<<'\n';
    require(visible>image->width()*image->height()/4&&mean<5.0,"Filtered icon differs from the original rendering");
}
void compare_design_reference(const std::shared_ptr<const ii::gui::RasterImage>& image,const char* filename){
    std::ifstream file(std::string(LVUI_DESIGN_ICON_REFERENCE_DIR)+"/"+filename,std::ios::binary);
    require(bool(file),"Missing Figma icon reference image");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    auto reference=take(ii::gui::png_image(bytes));
    require(reference->width()==image->width()&&reference->height()==image->height(),"Figma icon extent differs");
    const auto expected=reference->rgba(),actual=image->rgba();
    std::uint64_t difference=0,expected_alpha=0,actual_alpha=0;
    double expected_x=0,expected_y=0,actual_x=0,actual_y=0;
    for(std::size_t i=0;i<actual.size();i+=4){
        const auto pixel=i/4,x=pixel%image->width(),y=pixel/image->width();
        const auto ea=expected[i+3],aa=actual[i+3];
        expected_alpha+=ea;actual_alpha+=aa;expected_x+=double(x)*ea;expected_y+=double(y)*ea;
        actual_x+=double(x)*aa;actual_y+=double(y)*aa;difference+=std::abs(int(ea)-int(aa));
        for(unsigned channel=0;channel<3;++channel)
            difference+=std::abs((int(expected[i+channel])*int(ea)+127)/255-
                                 (int(actual[i+channel])*int(aa)+127)/255);
    }
    require(expected_alpha>0 && actual_alpha>0,"Figma icon reference is empty");
    const auto dx=std::abs(expected_x/double(expected_alpha)-actual_x/double(actual_alpha));
    const auto dy=std::abs(expected_y/double(expected_alpha)-actual_y/double(actual_alpha));
    const auto mean=double(difference)/double(actual.size());
    std::cout<<"DESIGN_ICON_REFERENCE "<<filename<<" MEAN "<<mean<<" CENTROID "<<dx<<','<<dy<<'\n';
    require(mean<5.0 && dx<0.75 && dy<0.75,"Design icon differs from the Figma reference");
}
}
int main(){
    lvui::native::NativeIconCache<ii::gui::CachedSvgDecoder> cache;
    auto builtin=take(cache.resolve("generaladd",{},18));require(builtin->width()==36&&builtin->height()==36,
        "Built-in icon raster extent is incorrect");
    lvui::native::NativeIconCache<ii::gui::CachedSvgDecoder> search_cache;
    auto search=take(search_cache.resolve("searchField",{},12));
    require(search->width()==24&&search->height()==24,"Search icon did not rasterize at its requested size");
    bool search_visible=false;auto search_bytes=search->rgba();
    for(std::size_t i=3;i<search_bytes.size();i+=4)search_visible|=search_bytes[i]!=0;
    require(search_visible,"The Figma search icon raster is empty");
    bool visible=false;auto bytes=builtin->rgba();for(std::size_t i=3;i<bytes.size();i+=4)visible|=bytes[i]!=0;
    require(visible,"Built-in icon raster is empty");
    auto again=take(cache.resolve("generaladd",{},18));require(builtin==again,"Identical icon request missed its cache");
    auto migrated=take(cache.resolve("generalopen",{},18));
    require(migrated->width()==36 && migrated->height()==36,
        "Authored icon fallback did not produce the requested raster");
    auto authored=lvui::authored_icon_catalog();
    auto preserved=take(cache.resolve("generaladd",authored,18));
    require(preserved->width()==36 && preserved->height()==36,
        "Explicit authored icon source did not render");
    auto legacy_icon=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("add",48));
    auto legacy_alias=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("generaladd",48));
    require(legacy_icon==legacy_alias && legacy_icon->width()==48,"Authored aliases did not share the raster cache");
    auto design_brush=take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("brush",36));
    auto design_brush_alias=take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("/brush.SVG",36));
    auto previous_brush=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("brush",36));
    require(design_brush==design_brush_alias && design_brush!=previous_brush &&
            !std::ranges::equal(design_brush->rgba(),previous_brush->rgba()),
            "Design icon raster did not preserve the Figma viewport and aliases");
    auto design_previous=take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("previous-frame",36));
    auto previous_default=take(cache.resolve("previous-frame",{},18));
    require(std::ranges::equal(design_previous->rgba(),previous_default->rgba()),
            "Default icon fallback does not use the LVUI design source");
    compare_design_reference(design_brush,"brush-36.png");
    compare_design_reference(design_previous,"previous-frame-36.png");
    compare_design_reference(take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("inspectionsEye",36)),"inspectionsEye-36.png");
    auto fallback_design=take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedDisabled",24));
    auto fallback_authored=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedDisabled",24));
    require(fallback_design==fallback_authored,
            "Design icon fallback did not preserve the authored filtered asset");
    for(const auto name:lvui::design_icon_names()) {
        auto image=lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>(name,36);
        if(!image || (*image)->rgba().size()!=36*36*4)
            throw std::runtime_error("Design icon could not render: "+std::string(name));
    }
    auto changing_source=std::make_shared<Churn>();
    auto changing=lvui::native::icon_image<ii::gui::CachedSvgDecoder>(changing_source,"x",36);
    require(!changing && changing.error().code==ii::core::ErrorCode::busy && changing_source->version==4,
            "An unstable raster source did not stop after bounded retries");
    auto unknown=lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("missing-icon-for-test");
    require(!unknown && unknown.error().code==ii::core::ErrorCode::not_found &&
            !lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("add",0) && !lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("add",513),
            "Authored raster lookup accepted an invalid request");
    std::vector<std::shared_ptr<const ii::gui::RasterImage>> shared(8);
    std::vector<std::thread> workers;
    for(std::size_t i=0;i<shared.size();++i)workers.emplace_back([&,i]{
        auto icon=lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("nodesfolder",48);
        if(icon)shared[i]=*icon;
    });
    for(auto& worker:workers)worker.join();
    require(shared[0] && std::ranges::all_of(shared,[&](const auto& image){return image==shared[0];}),
            "Concurrent authored raster requests did not converge");
    std::size_t rendered=0, unsupported=0, adapted=0;
    for(const auto name:lvui::authored_icon_names()) {
        auto svg=lvui::authored_icon_svg(name);
        require(bool(svg),"Authored icon bytes disappeared");
        auto image=ii::gui::svg_image(*svg,24,24);
        if(image) {
            require((*image)->rgba().size()==24*24*4,"Authored icon raster dimensions are incorrect");
            ++rendered;
        } else {
            if(image.error().code!=ii::core::ErrorCode::unsupported)
                throw std::runtime_error("Authored icon could not decode: "+std::string(name));
            ++unsupported;
        }
        auto native=lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>(name,24);
        if(!native||(*native)->rgba().size()!=24*24*4)
            throw std::runtime_error("Authored icon could not render through the native adapter: "+std::string(name));
        ++adapted;
    }
    require(rendered==2859 && unsupported==1 && adapted==2860,"Unexpected authored SVG rendering coverage");
    std::cout<<"RAW_SVG_RENDERED "<<rendered<<" RAW_UNSUPPORTED "<<unsupported<<" NATIVE_ADAPTER_RENDERED "<<adapted<<'\n';
    auto filtered=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedDisabled",24));
    auto filtered_default=take(cache.resolve("checkboxCheckedDisabled",{},12));
    auto filtered_explicit=take(cache.resolve("checkboxCheckedDisabled",authored,12));
    require(filtered->width()==24&&filtered->height()==24&&filtered_default->rgba().size()==24*24*4&&
            std::ranges::equal(filtered->rgba(),filtered_default->rgba())&&std::ranges::equal(filtered->rgba(),filtered_explicit->rgba()),
        "The filtered authored icon did not render consistently through its native adapters");
    compare_reference(filtered,"reference-24.png");
    compare_reference(take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedDisabled",64)),"reference-64.png");
    auto original=lvui::authored_icon_svg("checkboxCheckedDisabled");require(bool(original),"Original filtered SVG was lost");
    auto custom_filtered=lvui::IconCatalog::create({{"checkboxCheckedDisabled",std::string(*original)}});
    require(bool(custom_filtered),"Custom filtered source setup failed");
    auto unsupported_custom=cache.resolve("checkboxCheckedDisabled",std::static_pointer_cast<const lvui::IconSource>(*custom_filtered),12);
    require(!unsupported_custom&&unsupported_custom.error().code==ii::core::ErrorCode::unsupported,
        "An authored fallback silently replaced a custom icon source");
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
