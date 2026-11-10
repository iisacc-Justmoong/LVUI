#include "NativeImageLoader.h"
#include <LVUI/AuthoredIcons.h>
#include <iiGUI/Image.h>
#include <iiNetwork/Wire.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include <iiVisualCodec/ImageCodec.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
constexpr std::string_view svg=R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="red"/></svg>)";
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}

struct Request final:lvui::native::ImageRequest {
    lvui::native::ImageCompletion completion;
    std::function<void()> on_cancel;
    bool cancelled=false;
    explicit Request(lvui::native::ImageCompletion callback):completion(std::move(callback)){}
    void cancel()noexcept override{cancelled=true;if(on_cancel)on_cancel();}
};
struct Source final:lvui::native::ImageResourceSource {
    int reads=0;
    bool inline_reply=false,throw_read=false,throw_fetch=false;
    std::function<void()> during_read;
    std::map<std::string,std::string> payloads,canonical;
    std::vector<std::shared_ptr<Request>> requests;
    ii::core::Result<lvui::native::ImageResource> read(std::string_view address)override{
        ++reads;
        if(throw_read)throw std::runtime_error("source read failed");
        if(during_read)during_read();
        if(auto found=payloads.find(std::string(address));found!=payloads.end())return lvui::native::ImageResource{std::string(address),found->second};
        if(address.starts_with("https:")){
            const auto found=canonical.find(std::string(address));
            return lvui::native::ImageResource{found==canonical.end()?std::string(address):found->second,std::nullopt};
        }
        return lvui::native::ImageResource{std::string(address),std::string(address=="bad"?"not an image":svg)};
    }
    ii::core::Result<std::shared_ptr<lvui::native::ImageRequest>> fetch(std::string_view,lvui::native::ImageCompletion completion)override{
        if(throw_fetch)throw std::runtime_error("source fetch failed");
        auto request=std::make_shared<Request>(std::move(completion));
        requests.push_back(request);
        if(inline_reply)request->completion(std::string(svg));
        return std::shared_ptr<lvui::native::ImageRequest>(request);
    }
};
using Status=lvui::native::ImageLoadStatus;

void cache_and_raster(){
    auto source=std::make_shared<Source>();
    lvui::native::ImageLoader loader(source);
    auto first=loader.load("local.svg",12,6);
    require(first.status==Status::ready&&first.image&&first.image->width()==12&&first.image->height()==6,"SVG did not rasterize at the requested size");
    auto again=loader.load("local.svg",12,6);
    require(again.image==first.image&&source->reads==1,"Repeated source lost its cached image identity");
    auto resized=loader.load("local.svg",24,12);
    require(resized.image&&resized.image->width()==24&&resized.image!=first.image&&source->reads==1,"A second vector size reread the original source");
    loader.invalidate("local.svg");
    require(loader.load("local.svg",12,6).image!=first.image&&source->reads==2,"Invalidation retained a stale source");
    require(loader.load("").status==Status::empty,"An empty image source was treated as an error");
    require(loader.load("bad").status==Status::failed,"An invalid image payload was accepted");
}

void raster_formats_and_eviction(){
    auto source=std::make_shared<Source>();
    const std::vector<std::uint8_t> rgb{255,0,0,0,255,0};
    const ii::visual::image::ImageView view{2,1,ii::visual::image::PixelFormat::rgb8,0,rgb};
    const auto png=ii::visual::image::encode_png(view),jpeg=ii::visual::image::encode_jpeg(view);
    require(png&&jpeg,"Unable to produce image codec fixtures");
    source->payloads.emplace("raster.png",std::string(reinterpret_cast<const char*>(png->data()),png->size()));
    source->payloads.emplace("raster.jpg",std::string(reinterpret_cast<const char*>(jpeg->data()),jpeg->size()));
    lvui::native::ImageLoader loader(source,{.entries=2});
    auto png_image=loader.load("raster.png",32,32);
    auto jpeg_image=loader.load("raster.jpg",32,32);
    require(png_image.image&&jpeg_image.image&&png_image.image->width()==2&&png_image.image->height()==1&&jpeg_image.image->width()==2,
            "Raster decoding did not preserve image dimensions");
    require(loader.load("raster.png",32,32).image==png_image.image,"Cache access did not retain raster identity");
    require(loader.load("third.svg").status==Status::ready,"Third image failed");
    const auto before=source->reads;
    require(loader.load("raster.jpg").status==Status::ready&&source->reads==before+1,"Least recently used image was not evicted");
}

void remote_lifetime(){
    auto source=std::make_shared<Source>();
    lvui::native::ImageLoader loader(source);
    const auto url="https://example.test/image";
    require(loader.load(url).status==Status::loading&&loader.load(url).status==Status::loading&&source->requests.size()==1,"Pending loads were not coalesced");
    const auto before=loader.revision();
    source->requests[0]->completion(std::string(svg));
    require(loader.revision()>before&&loader.load(url).status==Status::ready,"Remote completion did not publish the image");
    const auto completed=loader.revision();
    source->requests[0]->completion(std::string(svg));
    require(loader.revision()==completed,"A duplicate completion published twice");
    loader.invalidate(url);
    require(loader.load(url).status==Status::loading&&source->requests.size()==2,"Invalidation did not begin a fresh request");
    auto old=source->requests[1];
    loader.invalidate(url);
    require(old->cancelled,"Invalidation did not cancel its request");
    require(loader.load(url).status==Status::loading&&source->requests.size()==3,"Replacement request was not admitted");
    old->completion(std::string(svg));
    require(loader.load(url).status==Status::loading,"A retired response replaced the current request");
    source->requests[2]->completion(ii::core::fail(ii::core::ErrorCode::failed,"offline"));
    auto failure=loader.load(url);
    require(failure.status==Status::failed&&failure.error.find("offline")!=std::string::npos,"Remote failure was not retained");
    require(loader.load(url).status==Status::failed&&source->requests.size()==3,"A failed load retried without an explicit invalidation");
    loader.invalidate(url);
    require(loader.load(url).status==Status::loading&&source->requests.size()==4,"Retry after invalidation failed");
}

void canonical_and_reentry(){
    auto source=std::make_shared<Source>();
    source->canonical.emplace("https://example.test/a","https://example.test/canonical");
    source->canonical.emplace("https://example.test/b","https://example.test/canonical");
    lvui::native::ImageLoader loader(source);
    require(loader.load("https://example.test/a").status==Status::loading&&loader.load("https://example.test/b").status==Status::loading&&source->requests.size()==1,
            "Equivalent source keys did not coalesce");
    source->requests[0]->completion(std::string(svg));
    require(loader.load("https://example.test/a").image==loader.load("https://example.test/b").image,"Equivalent keys did not share a raster");
    loader.invalidate("https://example.test/b");
    require(loader.load("https://example.test/a").status==Status::loading&&source->requests.size()==2,"Alias invalidation retained old bytes");
    source->during_read=[&]{loader.invalidate("not-cached");};
    require(loader.load("reentry.svg").status==Status::failed,"A reentrant invalidation published stale source data");
    source->during_read={};
    require(loader.load("reentry.svg").status==Status::ready,"Source did not recover after reentrant invalidation");
}

void invalid_sources_and_threads(){
    auto source=std::make_shared<Source>();
    lvui::native::ImageLoader loader(source,{.pending_requests=1});
    source->inline_reply=true;
    auto inline_result=loader.load("https://example.test/inline");
    require(inline_result.status==Status::failed&&source->requests.back()->cancelled,"Inline delivery was accepted as deferred");
    source->inline_reply=false;
    require(loader.load("https://example.test/first").status==Status::loading,"First pending request failed");
    require(loader.load("https://example.test/second").status==Status::failed,"Pending request budget was ignored");
    auto live=source->requests.back();
    std::async(std::launch::async,[live]{live->completion(std::string(svg));}).get();
    require(loader.load("https://example.test/first").status==Status::loading,"Foreign-thread completion mutated owner state");
    live->completion(std::string(svg));
    require(loader.load("https://example.test/first").status==Status::ready,"Owner-thread completion was lost");
    require(std::async(std::launch::async,[&]{try{(void)loader.load("local");return false;}catch(const std::logic_error&){return true;}}).get(),"Foreign-thread read was accepted");
    require(loader.load("local",0,1).status==Status::failed&&loader.load("local",8193,1).status==Status::failed,"Invalid vector size was accepted");
    source->throw_read=true;
    require(loader.load("exception").status==Status::failed,"A source exception escaped its contract");
}

void retirement(){
    auto source=std::make_shared<Source>();
    auto loader=std::make_unique<lvui::native::ImageLoader>(source);
    require(loader->load("https://example.test/pending").status==Status::loading,"Request did not start");
    auto request=source->requests.front();
    loader.reset();
    require(request->cancelled,"Retiring a loader did not cancel its request");
    request->completion(std::string(svg));
}

void cancellation_reentry(){
    auto source=std::make_shared<Source>();
    lvui::native::ImageLoader loader(source);
    require(loader.load("https://example.test/old").status==Status::loading,"Original request did not start");
    auto old=source->requests.front();
    old->on_cancel=[&]{(void)loader.load("https://example.test/replacement");};
    loader.clear();
    require(old->cancelled&&source->requests.size()==2&&!source->requests.back()->cancelled,
            "Reentrant request was cancelled by an earlier clear");
    old->completion(std::string(svg));
    require(loader.load("https://example.test/replacement").status==Status::loading,"Cleared completion published stale image data");
    source->requests.back()->completion(std::string(svg));
    require(loader.load("https://example.test/replacement").status==Status::ready,"Reentrant replacement could not finish");
}

void completion_reentry(){
    struct ReleasingRequest final:lvui::native::ImageRequest {
        std::function<void()> released;
        explicit ReleasingRequest(std::function<void()> value):released(std::move(value)){}
        ~ReleasingRequest()override{if(released)released();}
        void cancel()noexcept override{}
    };
    struct ReleasingSource final:lvui::native::ImageResourceSource {
        lvui::native::ImageCompletion completion;
        std::function<void()> released;
        unsigned starts=0;
        ii::core::Result<lvui::native::ImageResource> read(std::string_view address)override{return lvui::native::ImageResource{std::string(address),std::nullopt};}
        ii::core::Result<std::shared_ptr<lvui::native::ImageRequest>> fetch(std::string_view,lvui::native::ImageCompletion callback)override{
            ++starts;completion=std::move(callback);return std::shared_ptr<lvui::native::ImageRequest>(std::make_shared<ReleasingRequest>(released));
        }
    };
    auto source=std::make_shared<ReleasingSource>();
    lvui::native::ImageLoader loader(source);
    source->released=[&]{loader.clear();};
    require(loader.load("https://example.test/releasing").status==Status::loading,"Release test request did not start");
    const auto before=loader.revision();
    source->completion(std::string(svg));
    require(loader.revision()==before+1,"Retired completion emitted a second stale change");
    source->released={};
    require(loader.load("https://example.test/releasing").status==Status::loading&&source->starts==2,"Reentrant retirement left a stale cache entry");
}

void native_data_and_file(){
    lvui::native::ImageLoader loader;
    auto data="data:image/svg+xml;base64,"+ii::network::wire::base64(svg);
    require(loader.load(data,10,5).status==Status::ready,"SVG data URL failed");
    const auto path=std::filesystem::path(LVUI_IMAGE_RESOURCE_DIR)/"images/color-picker-hue-ring.png";
    auto image=loader.load(path.string());
    require(image.status==Status::ready&&image.image&&image.image->width()>1,"Native PNG file load failed");
    auto url="file://"+path.generic_string();
    require(loader.load(url).status==Status::ready,"Native file URL failed");
    const auto spaced=std::filesystem::path(LVUI_IMAGE_RESOURCE_DIR)/"authored-icons/css selectorname@14x14.svg";
    auto encoded=spaced.generic_string();
    for(std::size_t i=0;(i=encoded.find(' ',i))!=encoded.npos;i+=3)encoded.replace(i,1,"%20");
    require(loader.load("file://"+encoded).status==Status::ready,"A percent-encoded file URL failed");
    const auto names=lvui::authored_icon_names();
    require(!names.empty(),"Authored icon catalog is empty");
    const auto icon="/qt/qml/LVRS/resources/iconset/"+std::string(names.front())+".svg";
    require(loader.load("qrc:"+icon,16,16).status==Status::ready&&loader.load(":"+icon,16,16).status==Status::ready,
            "Legacy authored icon resource aliases failed");
    require(loader.load("https://example.test/image").status==Status::failed,"A remote source without an EventLoop was accepted");
    require(loader.load("http://user:password@example.test/image").status==Status::failed,"A credentialed image URL was accepted");
    auto unknown=loader.load("custom:unrecognized");
    require(unknown.status==Status::failed&&unknown.error.find("scheme")!=std::string::npos,"An unknown URL scheme was treated as a file");
    auto custom=lvui::native::native_image_source(nullptr,{.resource=[](std::string_view address)->ii::core::Result<std::string>{
        if(address=="assets://logo")return std::string(svg);
        return ii::core::fail(ii::core::ErrorCode::not_found,"Missing application resource");
    }});
    lvui::native::ImageLoader app_resources(custom);
    require(app_resources.load("assets://logo",10,5).status==Status::ready,"Application resource namespace was not routed to its provider");
    require(loader.load("data:image/png;base64,%%%invalid").status==Status::failed,"Malformed data URL was accepted");
}

void http(std::string_view mode,std::string url){
    ii::runtime::EventLoop loop;
    ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto loader=mode.starts_with("gzip_")?
        std::make_unique<lvui::native::ImageLoader>(loop,runtime,lvui::native::ImageSourceOptions{.maximum_source_bytes=512}):
        std::make_unique<lvui::native::ImageLoader>(loop,lvui::native::ImageSourceOptions{.maximum_source_bytes=512});
    require(loader->load(url,20,10).status==Status::loading,"HTTP load did not start asynchronously");
    const auto deadline=ii::runtime::Clock::now()+8s;
    auto result=loader->load(url,20,10);
    while(result.status==Status::loading&&ii::runtime::Clock::now()<deadline){loop.poll();std::this_thread::sleep_for(1ms);result=loader->load(url,20,10);}
    if(mode=="success"||mode=="redirect"||mode=="gzip_async"||mode=="gzip_http"||mode=="gzip_double")require(result.status==Status::ready&&result.image&&result.image->width()==20,"HTTP image did not load");
    else require(result.status==Status::failed&&!result.error.empty(),"HTTP failure did not finish with an error");
}
}
int main(int argc,char** argv){
    try{
        if(argc>1&&std::string_view(argv[1])=="http"){require(argc==4,"HTTP test arguments missing");http(argv[2],argv[3]);}
        else{cache_and_raster();raster_formats_and_eviction();remote_lifetime();canonical_and_reentry();invalid_sources_and_threads();retirement();cancellation_reentry();completion_reentry();native_data_and_file();}
        std::cout<<"Native image loader tests passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
