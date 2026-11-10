#include "NativeImageLoader.h"
#include <iiGUI/Image.h>
#include <iiNetwork/Wire.h>
#include <iiSDKCore/Core/Property/ObservableReadScope.h>
#include <iiSDKCore/Runtime/EventLoop.h>
#include <iiSDKCore/Runtime/Runtime.h>
#include "../example/Integrations/App/NativeImageAsync.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
using Status=lvui::native::ImageLoadStatus;
constexpr std::string_view red=R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="#ff0000"/></svg>)";
constexpr std::string_view green=R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="#00ff00"/></svg>)";
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}

struct Gate final {
    std::mutex mutex;
    std::condition_variable condition;
    unsigned started=0;
    bool released=false;
    void enter(){std::unique_lock lock(mutex);++started;condition.notify_all();condition.wait(lock,[&]{return released;});}
    bool wait_started(){std::unique_lock lock(mutex);return condition.wait_for(lock,10s,[&]{return started>0;});}
    void release(){std::lock_guard lock(mutex);released=true;condition.notify_all();}
    ~Gate(){release();}
};
struct Release final {
    std::vector<Gate*> gates;
    ~Release(){for(auto* gate:gates)gate->release();}
};
struct Reader final:lvui::native::ImageFileReader {
    Gate gate;
    const std::thread::id owner=std::this_thread::get_id();
    std::atomic<bool> on_owner=false;
    std::atomic<unsigned> reads=0;
    ii::core::Result<std::string> read(const std::filesystem::path&,std::size_t)override{
        if(std::this_thread::get_id()==owner)on_owner=true;++reads;gate.enter();return std::string(red);
    }
};
struct Decoder final:lvui::native::ImageDecoder {
    Gate gate;
    const std::thread::id owner=std::this_thread::get_id();
    std::atomic<bool> on_owner=false;
    std::atomic<unsigned> calls=0;
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> decode(
        std::string_view bytes,std::uint32_t width,std::uint32_t height)override{
        if(std::this_thread::get_id()==owner)on_owner=true;++calls;gate.enter();
        return lvui::native::native_image_decoder()->decode(bytes,width,height);
    }
};
struct InlineSource final:lvui::native::ImageResourceSource {
    ii::core::Result<lvui::native::ImageResource> read(std::string_view key)override{
        return lvui::native::ImageResource{std::string(key),std::string(red)};
    }
    ii::core::Result<std::shared_ptr<lvui::native::ImageRequest>> fetch(
        std::string_view,lvui::native::ImageCompletion)override{
        return ii::core::fail(ii::core::ErrorCode::failed,"Unexpected fetch");
    }
};
template<class Predicate>bool pump(ii::runtime::EventLoop& loop,Predicate predicate){
    const auto deadline=std::chrono::steady_clock::now()+10s;
    while(std::chrono::steady_clock::now()<deadline){if(predicate())return true;loop.poll();std::this_thread::sleep_for(1ms);}
    return predicate();
}

void background_pipeline(){
    ii::runtime::EventLoop loop;
    ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto reader=std::make_shared<Reader>();auto decoder=std::make_shared<Decoder>();
    lvui::native::ImageLoader loader(loop,runtime,{.file_reader=reader},{},decoder);
    Release release{{&reader->gate,&decoder->gate}};
    require(loader.load("slow.svg",12,6).status==Status::loading,"File read was not deferred");
    require(reader->gate.wait_started(),"File reader did not start");
    require(!loop.next_deadline(),"Blocked image work scheduled an idle polling deadline");
    require(!reader->on_owner,"File read ran on the owner thread");
    bool dispatched=false;require(bool(loop.post([&]{dispatched=true;})),"Cannot post responsiveness probe");
    loop.poll();require(dispatched,"Blocked file read prevented owner dispatch");
    require(loader.load("slow.svg",12,6).status==Status::loading&&reader->reads==1,"Concurrent file requests were duplicated");
    reader->gate.release();
    require(pump(loop,[&]{(void)loader.load("slow.svg",12,6);std::lock_guard lock(decoder->gate.mutex);return decoder->gate.started>0;}),"Decoder did not start");
    require(!decoder->on_owner,"Image decode ran on the owner thread");
    require(loader.load("slow.svg",12,6).status==Status::loading&&decoder->calls==1,"Concurrent decode requests were duplicated");
    dispatched=false;require(bool(loop.post([&]{dispatched=true;})),"Cannot post decoder responsiveness probe");
    loop.poll();require(dispatched,"Blocked decode prevented owner dispatch");
    decoder->gate.release();
    require(pump(loop,[&]{return loader.load("slow.svg",12,6).status!=Status::loading;}),"Decode did not finish");
    auto image=loader.load("slow.svg",12,6);
    require(image.status==Status::ready&&image.image&&image.image->width()==12&&image.image->height()==6,"Asynchronous image was not published");
    require(loader.load("slow.svg",12,6).image==image.image,"Asynchronous raster cache identity changed");
}

struct ChangingReader final:lvui::native::ImageFileReader {
    Gate first;
    std::atomic<unsigned> reads{0};
    ii::core::Result<std::string> read(const std::filesystem::path&,std::size_t)override{
        if(++reads==1){first.enter();return std::string(red);}
        return std::string(green);
    }
};
void invalidate_in_flight(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto reader=std::make_shared<ChangingReader>();
    lvui::native::ImageLoader loader(loop,runtime,{.file_reader=reader});
    Release release{{&reader->first}};
    require(loader.load("changed.svg").status==Status::loading&&reader->first.wait_started(),"Initial file work did not start");
    loader.invalidate("changed.svg");
    require(loader.load("changed.svg").status==Status::loading,"Replacement work did not start");
    const bool ready=pump(loop,[&]{return loader.load("changed.svg").status==Status::ready;});
    auto replacement=loader.load("changed.svg");
    reader->first.release();
    (void)runtime.wait_idle();
    for(int i=0;i<30;++i){loop.poll();std::this_thread::sleep_for(1ms);}
    require(ready&&replacement.image&&replacement.image->rgba()[1]>200,"Replacement image did not finish");
    require(loader.load("changed.svg").image==replacement.image,"Retired file work replaced the new image");
}

void retirement_does_not_wait(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto reader=std::make_shared<Reader>();
    auto loader=std::make_unique<lvui::native::ImageLoader>(loop,runtime,lvui::native::ImageSourceOptions{.file_reader=reader});
    Release release{{&reader->gate}};
    require(loader->load("retire.svg").status==Status::loading&&reader->gate.wait_started(),"Retirement work did not start");
    loader.reset();
    reader->gate.release();
    require(bool(runtime.wait_idle()),"Retired worker did not stop");
    loop.poll();
}

struct ChangingDecoder final:lvui::native::ImageDecoder {
    Gate first;
    std::atomic<unsigned> calls=0;
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> decode(
        std::string_view,std::uint32_t width,std::uint32_t height)override{
        if(++calls==1){first.enter();return lvui::native::native_image_decoder()->decode(red,width,height);}
        return lvui::native::native_image_decoder()->decode(green,width,height);
    }
};
void invalidate_decoding(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto decoder=std::make_shared<ChangingDecoder>();auto source=std::make_shared<InlineSource>();
    lvui::native::ImageLoader loader(source,loop,runtime,{},decoder);Release release{{&decoder->first}};
    require(loader.load("change.svg").status==Status::loading&&decoder->first.wait_started(),"Initial decode did not start");
    loader.invalidate("change.svg");
    require(loader.load("change.svg").status==Status::loading,"Replacement decode did not start");
    const bool ready=pump(loop,[&]{return loader.load("change.svg").status==Status::ready;});
    auto replacement=loader.load("change.svg");decoder->first.release();(void)runtime.wait_idle();
    for(int i=0;i<30;++i){loop.poll();std::this_thread::sleep_for(1ms);}
    require(ready&&replacement.image&&replacement.image->rgba()[1]>200,"Replacement decode failed");
    require(loader.load("change.svg").image==replacement.image,"Retired decode replaced the new image");
}

void bounded_delivery_and_zero_cache(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=4,.capacity=16});
    auto decoder=std::make_shared<Decoder>();auto source=std::make_shared<InlineSource>();
    lvui::native::ImageLoader loader(source,loop,runtime,{},decoder);Release release{{&decoder->gate}};
    for(int i=0;i<6;++i)require(loader.load("item"+std::to_string(i)).status==Status::loading,"Image work was not queued");
    {std::unique_lock lock(decoder->gate.mutex);require(decoder->gate.condition.wait_for(lock,10s,[&]{return decoder->gate.started==2;}),"Two workers did not start");}
    require(decoder->calls==2,"Too many image workers started");
    decoder->gate.release();(void)runtime.wait_idle();
    require(decoder->calls==2,"Undelivered results did not bound background work");
    require(pump(loop,[&]{for(int i=0;i<6;++i)if(loader.load("item"+std::to_string(i)).status!=Status::ready)return false;return true;}),"Bounded queue did not drain");
    require(decoder->calls==6,"The queue skipped or duplicated work");
    lvui::native::ImageLoader uncached(source,loop,runtime,{.raster_bytes=0});
    require(uncached.load("uncached.svg").status==Status::loading,"Uncached decode ran inline");
    lvui::native::ImageLoad result;
    require(pump(loop,[&]{result=uncached.load("uncached.svg");return result.status!=Status::loading;}),"Uncached result was not delivered");
    require(result.status==Status::ready&&result.image,"Uncached result was lost");
    require(uncached.load("uncached.svg").status==Status::loading,"Zero-sized cache retained a raster");
}

void native_inputs(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    lvui::native::ImageLoader loader(loop,runtime);
    const auto data="data:image/svg+xml;base64,"+ii::network::wire::base64(red);
    const auto path=std::filesystem::path(LVUI_IMAGE_RESOURCE_DIR)/"images/color-picker-hue-ring.png";
    const auto spaced=std::filesystem::path(LVUI_IMAGE_RESOURCE_DIR)/"authored-icons/css selectorname@14x14.svg";
    auto encoded=spaced.generic_string();for(std::size_t i=0;(i=encoded.find(' ',i))!=encoded.npos;i+=3)encoded.replace(i,1,"%20");
    for(const auto& address:std::vector<std::string>{data,path.string(),"file://"+encoded}){
        require(loader.load(address).status==Status::loading,"Native input did not start in background");
        require(pump(loop,[&]{return loader.load(address).status!=Status::loading;}),"Native input did not finish");
        require(loader.load(address).status==Status::ready,"Native input failed");
    }
}

struct FailingDecoder final:lvui::native::ImageDecoder {
    std::atomic<unsigned> calls=0;
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> decode(
        std::string_view bytes,std::uint32_t width,std::uint32_t height)override{
        if(++calls==1)throw std::runtime_error("decoder failure");
        return lvui::native::native_image_decoder()->decode(bytes,width,height);
    }
};
struct OversizeReader final:lvui::native::ImageFileReader {
    ii::core::Result<std::string> read(const std::filesystem::path&,std::size_t maximum)override{
        return std::string(maximum+1,'x');
    }
};
void failures_and_retry(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=16});
    auto decoder=std::make_shared<FailingDecoder>();auto source=std::make_shared<InlineSource>();
    lvui::native::ImageLoader loader(source,loop,runtime,{},decoder);
    require(loader.load("failure.svg").status==Status::loading,"Throwing decoder was not queued");
    require(pump(loop,[&]{return loader.load("failure.svg").status!=Status::loading;}),"Decoder exception never completed");
    auto failure=loader.load("failure.svg");
    require(failure.status==Status::failed&&failure.error.find("decoder failure")!=std::string::npos&&decoder->calls==1,"Decoder exception was not retained");
    loader.invalidate("failure.svg");
    require(loader.load("failure.svg").status==Status::loading,"Invalidation did not retry a failed decode");
    require(pump(loop,[&]{return loader.load("failure.svg").status!=Status::loading;}),"Retried decode never completed");
    require(loader.load("failure.svg").status==Status::ready&&decoder->calls==2,"Retried decode did not recover");
    auto reader=std::make_shared<OversizeReader>();
    lvui::native::ImageLoader bounded(loop,runtime,{.maximum_source_bytes=32,.file_reader=reader});
    require(bounded.load("large.svg").status==Status::loading,"Oversized source was not queued");
    require(pump(loop,[&]{return bounded.load("large.svg").status!=Status::loading;}),"Oversized source never completed");
    require(bounded.load("large.svg").status==Status::failed,"A custom reader bypassed the byte limit");
}

void stopped_owner(){
    ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=2,.capacity=8});
    auto source=std::make_shared<InlineSource>();auto decoder=std::make_shared<Decoder>();
    lvui::native::ImageLoader loader(source,loop,runtime,{},decoder);Release release{{&decoder->gate}};
    require(loader.load("stopping.svg").status==Status::loading&&decoder->gate.wait_started(),"Owner stop work did not start");
    loop.request_stop();
    require(loader.load("stopping.svg").status==Status::failed,"A stopped owner left work loading forever");
    decoder->gate.release();(void)runtime.wait_idle();
}

struct PartiallyBlockedDecoder final:lvui::native::ImageDecoder {
    Gate second;
    std::atomic<unsigned> calls=0;
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> decode(
        std::string_view bytes,std::uint32_t width,std::uint32_t height)override{
        if(++calls==2)second.enter();
        return lvui::native::native_image_decoder()->decode(bytes,width,height);
    }
};
void observer_failure(std::size_t capacity){
    ii::runtime::EventLoop loop(capacity);ii::runtime::Runtime runtime({.workers=2,.capacity=8});
    auto source=std::make_shared<InlineSource>();auto decoder=std::make_shared<PartiallyBlockedDecoder>();
    lvui::native::ImageLoader loader(source,loop,runtime,{},decoder);Release release{{&decoder->second}};
    ii::core::ObservableRead read;
    ii::core::ObservableReadScope::collect([&](auto value){read=std::move(value);},[&]{(void)loader.revision();});
    unsigned notifications=0;
    auto subscription=read.subscribe([&]{if(++notifications==1)throw std::runtime_error("image observer failed");});
    require(loader.load("observer-a.svg").status==Status::loading&&loader.load("observer-b.svg").status==Status::loading,
            "Observer failure requests did not start");
    require(decoder->second.wait_started(),"Second observer test decode did not start");
    std::string observed;
    const auto dispatch=[&]{try{loop.poll();}catch(const std::runtime_error& error){observed=error.what();}};
    auto deadline=std::chrono::steady_clock::now()+10s;
    while(!notifications&&std::chrono::steady_clock::now()<deadline){dispatch();std::this_thread::sleep_for(1ms);}
    require(notifications>0,"Observer was not notified");
    if(capacity==1)require(observed.empty(),"Observer failure interrupted another image when the owner queue was full");
    else{
        while(observed.empty()&&std::chrono::steady_clock::now()<deadline){dispatch();std::this_thread::sleep_for(1ms);}
        require(observed=="image observer failed","Observer failure was not delivered while another image was running");
    }
    decoder->second.release();
    deadline=std::chrono::steady_clock::now()+10s;
    while((observed.empty()||loader.load("observer-a.svg").status!=Status::ready||loader.load("observer-b.svg").status!=Status::ready)
          &&std::chrono::steady_clock::now()<deadline){dispatch();std::this_thread::sleep_for(1ms);}
    require(observed=="image observer failed","Observer failure was lost when the owner queue was full");
    require(loader.load("observer-a.svg").status==Status::ready&&loader.load("observer-b.svg").status==Status::ready,
            "Observer failure prevented another image from completing");
    require(loader.load("observer-c.svg").status==Status::loading,"Image queue did not accept work after an observer failure");
    require(pump(loop,[&]{return loader.load("observer-c.svg").status==Status::ready;}),"Image queue did not recover after an observer failure");
}

void observer_retires_loader(bool destroy){
    ii::runtime::EventLoop loop(1);ii::runtime::Runtime runtime({.workers=2,.capacity=8});
    auto source=std::make_shared<InlineSource>();auto decoder=std::make_shared<PartiallyBlockedDecoder>();
    auto loader=std::make_unique<lvui::native::ImageLoader>(source,loop,runtime,lvui::native::ImageCacheLimits{},decoder);
    Release release{{&decoder->second}};ii::core::ObservableRead read;
    ii::core::ObservableReadScope::collect([&](auto value){read=std::move(value);},[&]{(void)loader->revision();});
    unsigned notifications=0;
    auto subscription=read.subscribe([&]{
        if(++notifications!=1)return;
        if(destroy)loader.reset();else loader->clear();
        throw std::runtime_error("retired image observer failed");
    });
    require(loader->load("retire-a.svg").status==Status::loading&&loader->load("retire-b.svg").status==Status::loading,
            "Retiring observer work did not start");
    require(decoder->second.wait_started(),"Retiring observer blocker did not start");
    std::string observed;const auto deadline=std::chrono::steady_clock::now()+10s;
    while(observed.empty()&&std::chrono::steady_clock::now()<deadline){
        try{loop.poll();}catch(const std::runtime_error& error){observed=error.what();}
        std::this_thread::sleep_for(1ms);
    }
    require(observed=="retired image observer failed","Retiring an observer lost its error behind cancelled work");
    if(!destroy){
        require(loader->load("after-clear.svg").status==Status::loading,"A cleared loader could not restart");
        require(pump(loop,[&]{return loader->load("after-clear.svg").status==Status::ready;}),"A cleared loader did not recover while old work retired");
    }
    decoder->second.release();(void)runtime.wait_idle();
}

void retired_signal_rebinds_running_jobs(){
    ii::runtime::EventLoop loop(1);ii::runtime::Runtime runtime({.workers=2,.capacity=8});
    auto queue=std::make_shared<lvui::native::detail::ImageWorkQueue>(loop,runtime,2);
    Gate old_work,new_work;Release release{{&old_work,&new_work}};
    std::shared_ptr<lvui::native::ImageRequest> old_request;
    auto first=queue->submit<int>([]{return ii::core::Result<int>(1);},[&](auto){
        old_request->cancel();throw std::runtime_error("retire signal");
    });
    require(bool(first),"Cannot submit initial image work");
    auto old=queue->submit<int>([&]{old_work.enter();return ii::core::Result<int>(2);},[](auto){});
    require(bool(old),"Cannot submit image work to retire");old_request=*old;
    require(old_work.wait_started(),"Old image work did not start");
    std::string error;
    const auto deadline=std::chrono::steady_clock::now()+10s;
    while(error.empty()&&std::chrono::steady_clock::now()<deadline){
        try{loop.poll();}catch(const std::runtime_error& e){error=e.what();}
        std::this_thread::sleep_for(1ms);
    }
    require(!error.empty(),"The retiring observer error was lost");
    require(error=="retire signal","Wrong retiring observer error");
    auto current=queue->submit<int>([&]{new_work.enter();return ii::core::Result<int>(3);},[](auto){});
    require(bool(current)&&new_work.wait_started(),"Cannot reserve a new completion signal");
    old_work.release();
    std::shared_ptr<lvui::native::ImageRequest> next;
    require(pump(loop,[&]{
        auto submitted=queue->submit<int>([]{return ii::core::Result<int>(4);},[](auto){});
        if(!submitted)return false;
        next=*submitted;return true;
    }),"A retired worker failed to signal the replacement slot");
    new_work.release();(void)runtime.wait_idle();
}

void saturated_loop_and_runtime_stop(){
    auto source=std::make_shared<InlineSource>();
    {
        ii::runtime::EventLoop loop(1);ii::runtime::Runtime runtime({.workers=1,.capacity=8});
        require(bool(loop.post([]{})),"Cannot saturate loop");
        lvui::native::ImageLoader loader(source,loop,runtime);
        require(loader.load("full.svg").status==Status::failed,"Full owner queue accepted image work");
        loop.poll();loader.invalidate("full.svg");
        require(loader.load("full.svg").status==Status::loading,"Image could not retry after owner queue recovered");
        require(pump(loop,[&]{return loader.load("full.svg").status!=Status::loading;}),"Recovered image never completed");
        require(loader.load("full.svg").status==Status::ready,"Recovered image failed");
    }
    {
        ii::runtime::EventLoop loop;ii::runtime::Runtime runtime({.workers=1,.capacity=8});Gate blocker;
        Release release{{&blocker}};
        require(bool(runtime.post([&]{blocker.enter();})),"Cannot occupy runtime");
        require(blocker.wait_started(),"Runtime blocker did not start");
        lvui::native::ImageLoader loader(source,loop,runtime);
        require(loader.load("stopped.svg").status==Status::loading,"Decode did not enter runtime queue");
        runtime.request_stop(ii::runtime::ShutdownMode::cancel_pending);
        const bool finished=pump(loop,[&]{return loader.load("stopped.svg").status!=Status::loading;});
        const auto result=loader.load("stopped.svg");
        blocker.release();(void)runtime.join();
        require(finished&&result.status==Status::failed,"Cancelled queued decode remained loading");
    }
}
}
int main(){
    try{background_pipeline();invalidate_in_flight();invalidate_decoding();retirement_does_not_wait();
        bounded_delivery_and_zero_cache();native_inputs();failures_and_retry();stopped_owner();
        observer_failure(1);observer_failure(4);observer_retires_loader(false);observer_retires_loader(true);
        retired_signal_rebinds_running_jobs();saturated_loop_and_runtime_stop();
        std::cout<<"Native asynchronous image tests passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
