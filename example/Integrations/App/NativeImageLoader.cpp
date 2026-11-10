#include "NativeImageLoader.h"
#include "NativeImageAsync.h"
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiVisualCodec/ImageCodec.h>

#include <algorithm>
#include <limits>
#include <list>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

namespace lvui::native {
namespace {
ImageLoad failed(std::string message){return {ImageLoadStatus::failed,{},std::move(message)};}
std::string bounded_error(std::string message){if(message.size()>4096)message.resize(4096);return message;}
using Pixels=std::shared_ptr<const ii::gui::RasterImage>;
ii::core::Result<Pixels> decode_native(std::string_view source,std::uint32_t width,std::uint32_t height){
    const auto data=std::span(reinterpret_cast<const std::uint8_t*>(source.data()),source.size());
    ii::visual::image::Limits limits;
    limits.input_bytes=64*1024*1024;limits.pixel_bytes=ii::gui::maximum_image_pixels*4;
    limits.dimension=ii::gui::maximum_image_dimension;
    if(ii::visual::image::inspect(data,limits)){
        auto image=ii::visual::image::decode(data,limits);
        if(!image)return ii::core::fail(ii::core::ErrorCode::invalid_argument,image.error().message);
        auto converted=ii::visual::image::to_srgb(std::move(*image),limits);
        if(!converted)return ii::core::fail(ii::core::ErrorCode::invalid_argument,converted.error().message);
        return ii::gui::rgba_image(converted->width,converted->height,std::move(converted->rgba));
    }
    return ii::gui::svg_image(source,width,height);
}
class NativeDecoder final:public ImageDecoder {
public:
    ii::core::Result<Pixels> decode(std::string_view source,std::uint32_t width,std::uint32_t height)override{
        return decode_native(source,width,height);
    }
};
}
std::shared_ptr<ImageDecoder> native_image_decoder(){return std::make_shared<NativeDecoder>();}
struct ImageLoader::Impl final:std::enable_shared_from_this<Impl> {
    struct Raster final {
        std::uint32_t width,height;Pixels image;std::uint64_t used;
        ImageLoadStatus status=ImageLoadStatus::ready;
        std::string error;
        std::shared_ptr<ImageRequest> request;
        std::uint64_t serial=0;
        bool transient=false;
    };
    struct Entry final {
        std::string key,error;
        std::shared_ptr<const std::string> bytes;
        std::vector<std::string> addresses;
        std::vector<Raster> rasters;
        ImageLoadStatus status=ImageLoadStatus::loading;
        std::shared_ptr<ImageRequest> request;
        std::uint64_t serial=0,used=0;
        bool admitting=false,inline_completion=false,valid=true;
    };
    std::shared_ptr<ImageResourceSource> source;
    ImageCacheLimits limits;
    std::shared_ptr<detail::ImageWorkQueue> work;
    std::shared_ptr<ImageDecoder> decoder;
    const std::thread::id owner=std::this_thread::get_id();
    std::list<std::shared_ptr<Entry>> entries;
    std::unordered_map<std::string,std::shared_ptr<Entry>> addresses,keys;
    ii::core::Observable<std::uint64_t> epoch{0};
    std::uint64_t sequence=0,generation=0,revision_value=0;
    bool closed=false,querying=false;

    Impl(std::shared_ptr<ImageResourceSource> source_,ImageCacheLimits limits_,
         std::shared_ptr<detail::ImageWorkQueue> work_={},std::shared_ptr<ImageDecoder> decoder_={})
        :source(std::move(source_)),limits(limits_),work(std::move(work_)),decoder(std::move(decoder_)){
        if(!source||!limits.source_bytes||limits.source_bytes>256ull*1024*1024||
           limits.raster_bytes>512ull*1024*1024||!limits.entries||limits.entries>4096||
           !limits.pending_requests||limits.pending_requests>4096)throw std::invalid_argument("Invalid image source or cache limits");
        if(work&&!decoder)decoder=native_image_decoder();
    }
    void owner_check()const{if(owner!=std::this_thread::get_id())throw std::logic_error("Image loader requires its owner thread");}
    std::uint64_t next(){if(sequence==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Image loader sequence exhausted");return ++sequence;}
    void changed(){
        if(closed)return;
        if(revision_value==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Image loader revision exhausted");
        epoch.set(++revision_value);
    }
    std::size_t source_cost()const{
        std::size_t total=0;
        for(const auto& entry:entries){total+=sizeof(Entry)+entry->key.size()*2+(entry->bytes?entry->bytes->size():0)+entry->error.size();for(const auto& address:entry->addresses)total+=address.size()*2;}
        return total;
    }
    std::size_t raster_cost()const{
        std::size_t total=0;for(const auto& entry:entries)for(const auto& raster:entry->rasters)if(raster.image&&!raster.transient)total+=sizeof(Raster)+raster.image->rgba().size();return total;
    }
    void erase(std::shared_ptr<Entry> entry){
        if(!entry||!entry->valid)return;
        entry->valid=false;
        for(const auto& address:entry->addresses){auto found=addresses.find(address);if(found!=addresses.end()&&found->second==entry)addresses.erase(found);}
        auto found=keys.find(entry->key);if(found!=keys.end()&&found->second==entry)keys.erase(found);
        entries.remove(entry);
        auto request=std::move(entry->request);
        std::vector<std::shared_ptr<ImageRequest>> raster_requests;
        for(auto& raster:entry->rasters)if(raster.request)raster_requests.push_back(std::move(raster.request));
        if(request)request->cancel();
        for(const auto& pending:raster_requests)pending->cancel();
    }
    bool fit_sources(const std::shared_ptr<Entry>& protected_entry){
        while(entries.size()>limits.entries||source_cost()>limits.source_bytes){
            std::shared_ptr<Entry> oldest;
            for(const auto& entry:entries)if(entry!=protected_entry&&entry->status!=ImageLoadStatus::loading&&(!oldest||entry->used<oldest->used))oldest=entry;
            if(!oldest)return false;
            erase(oldest);
        }
        return true;
    }
    void fit_rasters(){
        while(raster_cost()>limits.raster_bytes){
            std::shared_ptr<Entry> oldest;std::size_t index=0;std::uint64_t stamp=std::numeric_limits<std::uint64_t>::max();
            for(const auto& entry:entries)for(std::size_t i=0;i<entry->rasters.size();++i){const auto& raster=entry->rasters[i];if(raster.image&&!raster.transient&&raster.used<stamp){oldest=entry;index=i;stamp=raster.used;}}
            if(!oldest)return;
            oldest->rasters.erase(oldest->rasters.begin()+static_cast<std::ptrdiff_t>(index));
        }
    }
    std::size_t pending()const{
        return std::ranges::count_if(entries,[](const auto& entry){return entry->status==ImageLoadStatus::loading;});
    }
    void complete(const std::weak_ptr<Entry>& weak,std::uint64_t serial,ii::core::Result<std::string> result){
        if(owner!=std::this_thread::get_id()||closed)return;
        auto entry=weak.lock();if(!entry||!entry->valid||entry->serial!=serial)return;
        if(entry->admitting){entry->inline_completion=true;return;}
        if(entry->status!=ImageLoadStatus::loading)return;
        auto completed=std::move(entry->request);
        if(!result||result->empty()){
            entry->status=ImageLoadStatus::failed;
            entry->error=bounded_error(!result?(result.error().message.empty()?"Image request failed":result.error().message):"Received empty image payload");
        }else if(result->size()>64*1024*1024){
            entry->status=ImageLoadStatus::failed;entry->error="Image source exceeds its byte limit";
        }else{
            entry->bytes=std::make_shared<const std::string>(std::move(*result));entry->status=ImageLoadStatus::ready;
            if(!fit_sources(entry)){entry->bytes.reset();entry->status=ImageLoadStatus::failed;entry->error="Image source exceeds its cache budget";}
        }
        entry->used=next();
        completed.reset();
        if(closed||!entry->valid||entry->serial!=serial)return;
        changed();
    }
    void start(const std::shared_ptr<Entry>& entry){
        if(pending()>limits.pending_requests){entry->status=ImageLoadStatus::failed;entry->error="Image request capacity exhausted";return;}
        entry->admitting=true;entry->serial=next();const auto serial=entry->serial;
        const auto invoke=[&]()->ii::core::Result<std::shared_ptr<ImageRequest>>{
            try{return source->fetch(entry->key,[owner=weak_from_this(),weak=std::weak_ptr<Entry>(entry),serial](auto result){if(auto self=owner.lock())self->complete(weak,serial,std::move(result));});}
            catch(const std::exception& error){return ii::core::fail(ii::core::ErrorCode::failed,error.what(),std::current_exception());}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Image source request threw",std::current_exception());}
        };
        auto request=invoke();
        if(closed||!entry->valid){if(request&&*request)(*request)->cancel();return;}
        entry->admitting=false;
        if(!request||!*request||entry->inline_completion){
            entry->status=ImageLoadStatus::failed;
            entry->error=bounded_error(!request?(request.error().message.empty()?"Image request failed":request.error().message):"Image source completion must be deferred");
            if(request&&*request)(*request)->cancel();
            return;
        }
        entry->request=std::move(*request);
    }
    std::shared_ptr<Entry> find_or_read(std::string_view address){
        if(auto found=addresses.find(std::string(address));found!=addresses.end()){found->second->used=next();return found->second;}
        const auto before=generation;
        const auto read=[&]()->ii::core::Result<ImageResource>{
            try{return source->read(address);}
            catch(const std::exception& error){return ii::core::fail(ii::core::ErrorCode::failed,error.what(),std::current_exception());}
            catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Image source read threw",std::current_exception());}
        };
        auto value=read();
        if(closed||generation!=before)return {};
        auto entry=std::make_shared<Entry>();entry->used=next();entry->addresses.emplace_back(address);
        if(!value){entry->key=std::string(address);entry->status=ImageLoadStatus::failed;entry->error=bounded_error(value.error().message.empty()?"Image source read failed":value.error().message);}
        else{
            if(value->key.empty()||value->key.size()>64ull*1024*1024){entry->key=std::string(address);entry->status=ImageLoadStatus::failed;entry->error="Invalid image source key";}
            else{
                entry->key=std::move(value->key);
                if(auto existing=keys.find(entry->key);existing!=keys.end()){
                    auto shared=existing->second;
                    shared->addresses.emplace_back(address);addresses.emplace(shared->addresses.back(),shared);shared->used=next();
                    if(!fit_sources(shared)){addresses.erase(shared->addresses.back());shared->addresses.pop_back();return {};}
                    return shared;
                }
                if(value->bytes){
                    if(value->bytes->empty()||value->bytes->size()>64ull*1024*1024){entry->status=ImageLoadStatus::failed;entry->error="Invalid or oversized image payload";}
                    else{entry->bytes=std::make_shared<const std::string>(std::move(*value->bytes));entry->status=ImageLoadStatus::ready;}
                }
            }
        }
        entries.push_back(entry);addresses.emplace(entry->addresses.front(),entry);keys.emplace(entry->key,entry);
        if(!fit_sources(entry)){erase(entry);return {};}
        if(entry->status==ImageLoadStatus::loading)start(entry);
        return entry->valid?entry:std::shared_ptr<Entry>{};
    }
    void trim_transient_rasters(){
        for(;;){
            std::size_t count=0,index=0;std::uint64_t stamp=std::numeric_limits<std::uint64_t>::max();
            std::shared_ptr<Entry> oldest;
            for(const auto& entry:entries)for(std::size_t i=0;i<entry->rasters.size();++i){
                const auto& raster=entry->rasters[i];if(!raster.transient)continue;++count;
                if(raster.used<stamp){oldest=entry;index=i;stamp=raster.used;}
            }
            if(count<=2||!oldest)return;
            oldest->rasters.erase(oldest->rasters.begin()+static_cast<std::ptrdiff_t>(index));
        }
    }
    bool raster_slot(const std::shared_ptr<Entry>& entry){
        if(entry->rasters.size()<256)return true;
        auto oldest=entry->rasters.end();
        for(auto it=entry->rasters.begin();it!=entry->rasters.end();++it)
            if(it->status!=ImageLoadStatus::loading&&(oldest==entry->rasters.end()||it->used<oldest->used))oldest=it;
        if(oldest==entry->rasters.end())return false;
        entry->rasters.erase(oldest);return true;
    }
    void complete_raster(const std::weak_ptr<Entry>& weak,std::uint64_t serial,ii::core::Result<Pixels> result){
        if(owner!=std::this_thread::get_id()||closed)return;
        auto entry=weak.lock();if(!entry||!entry->valid)return;
        auto found=std::ranges::find_if(entry->rasters,[&](const auto& raster){return raster.serial==serial;});
        if(found==entry->rasters.end()||found->status!=ImageLoadStatus::loading)return;
        auto completed=std::move(found->request);found->used=next();
        if(!result||!*result){
            found->status=ImageLoadStatus::failed;
            found->error=bounded_error(!result?(result.error().message.empty()?"Image decoding failed":result.error().message):"Image decoder returned no image");
        }else{
            found->image=std::move(*result);found->status=ImageLoadStatus::ready;
            found->transient=found->image->rgba().size()+sizeof(Raster)>limits.raster_bytes;
            if(found->transient)trim_transient_rasters();else fit_rasters();
        }
        completed.reset();
        if(closed||!entry->valid)return;
        changed();
    }
    ImageLoad load(std::string_view address,std::uint32_t width,std::uint32_t height){
        owner_check();(void)epoch.get();
        if(closed)return failed("Image loader is closed");
        if(address.empty())return {};
        if(work&&!work->loop_available())return failed("Image owner loop is stopped");
        if(!width||!height||width>ii::gui::maximum_image_dimension||height>ii::gui::maximum_image_dimension||
           std::uint64_t(width)*height>ii::gui::maximum_image_pixels)return failed("Invalid image raster size");
        if(address.size()>64ull*1024*1024)return failed("Image address exceeds its byte limit");
        if(querying)throw std::logic_error("Recursive image query");
        querying=true;struct Guard final{bool& flag;~Guard(){flag=false;}}guard{querying};
        auto entry=find_or_read(address);
        if(!entry)return failed(closed?"Image loader is closed":"Image source changed or exceeded its cache budget");
        if(entry->status==ImageLoadStatus::failed)return failed(entry->error);
        if(entry->status==ImageLoadStatus::loading)return {ImageLoadStatus::loading,{},{}};
        for(auto it=entry->rasters.begin();it!=entry->rasters.end();++it)if(it->width==width&&it->height==height){
            it->used=next();
            if(it->status==ImageLoadStatus::loading)return {ImageLoadStatus::loading,{},{}};
            if(it->status==ImageLoadStatus::failed)return failed(it->error);
            auto image=it->image;if(it->transient)entry->rasters.erase(it);
            return {ImageLoadStatus::ready,std::move(image),{}};
        }
        if(work){
            if(!raster_slot(entry))return failed("Image raster request capacity exhausted");
            const auto serial=next();
            entry->rasters.push_back({width,height,{},next(),ImageLoadStatus::loading,{}, {},serial});
            auto submitted=work->submit<Pixels>([bytes=entry->bytes,decoder=decoder,width,height]{return decoder->decode(*bytes,width,height);},
                [owner=weak_from_this(),weak=std::weak_ptr<Entry>(entry),serial](auto result){if(auto self=owner.lock())self->complete_raster(weak,serial,std::move(result));},true);
            if(!submitted){
                auto& raster=entry->rasters.back();raster.status=ImageLoadStatus::failed;raster.error=bounded_error(submitted.error().message);
                return failed(raster.error);
            }
            entry->rasters.back().request=std::move(*submitted);
            return {ImageLoadStatus::loading,{},{}};
        }
        auto result=decode_native(*entry->bytes,width,height);
        if(!result)return failed(result.error().message.empty()?"Image decoding failed":result.error().message);
        if(closed||!entry->valid)return failed("Image source retired during decoding");
        auto image=std::move(*result);
        if(image->rgba().size()+sizeof(Raster)<=limits.raster_bytes){entry->rasters.push_back({width,height,image,next()});fit_rasters();}
        return {ImageLoadStatus::ready,std::move(image),{}};
    }
    void invalidate(std::string_view address){
        owner_check();if(closed)return;++generation;
        auto found=addresses.find(std::string(address));if(found==addresses.end())return;
        auto entry=found->second;erase(entry);changed();
    }
    void clear(){
        owner_check();if(closed)return;++generation;
        if(entries.empty())return;
        drop_all();
        changed();
    }
    void drop_all()noexcept{
        auto retired=std::move(entries);entries.clear();addresses.clear();keys.clear();
        for(const auto& entry:retired)entry->valid=false;
        for(const auto& entry:retired){
            for(auto& raster:entry->rasters){auto request=std::move(raster.request);if(request)request->cancel();}
            auto request=std::move(entry->request);if(request)request->cancel();
        }
    }
    void retire()noexcept{
        closed=true;
        drop_all();
    }
};
ImageLoader::ImageLoader():ImageLoader(native_image_source()){}
ImageLoader::ImageLoader(ii::runtime::EventLoop& loop,ImageSourceOptions options,ImageCacheLimits limits):ImageLoader(native_image_source(&loop,std::move(options)),limits){}
ImageLoader::ImageLoader(std::shared_ptr<ImageResourceSource> source,ImageCacheLimits limits):impl_(std::make_shared<Impl>(std::move(source),limits)){}
ImageLoader::ImageLoader(ii::runtime::EventLoop& loop,ii::runtime::Runtime& runtime,ImageSourceOptions options,ImageCacheLimits limits,std::shared_ptr<ImageDecoder> decoder){
    auto work=std::make_shared<detail::ImageWorkQueue>(loop,runtime,limits.pending_requests);
    auto source=detail::native_async_image_source(loop,work,std::move(options));
    impl_=std::make_shared<Impl>(std::move(source),limits,std::move(work),std::move(decoder));
}
ImageLoader::ImageLoader(std::shared_ptr<ImageResourceSource> source,ii::runtime::EventLoop& loop,ii::runtime::Runtime& runtime,ImageCacheLimits limits,std::shared_ptr<ImageDecoder> decoder)
    :impl_(std::make_shared<Impl>(std::move(source),limits,std::make_shared<detail::ImageWorkQueue>(loop,runtime,limits.pending_requests),std::move(decoder))){}
ImageLoader::~ImageLoader(){impl_->retire();}
ImageLoad ImageLoader::load(std::string_view address,std::uint32_t width,std::uint32_t height)const{auto state=impl_;return state->load(address,width,height);}
Pixels ImageLoader::image(std::string_view address,std::uint32_t width,std::uint32_t height)const{return load(address,width,height).image;}
std::uint64_t ImageLoader::revision()const{auto state=impl_;state->owner_check();return state->epoch.get();}
void ImageLoader::invalidate(std::string_view address){auto state=impl_;state->invalidate(address);}
void ImageLoader::clear(){auto state=impl_;state->clear();}
} // namespace lvui::native
