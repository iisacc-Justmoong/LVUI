#include "NativeImageLoader.h"
#include <LVUI/AuthoredIcons.h>
#include "NativeImageAsync.h"
#include <iiFileProviderNative.h>
#include <iiNetwork/Http.h>
#include <iiNetwork/Wire.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <curl/curl.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <utility>

namespace lvui::native {
namespace {
using ii::core::ErrorCode;
template<class T>using Result=ii::core::Result<T>;
std::string lower(std::string_view input){std::string result(input);for(auto& c:result)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return result;}
std::filesystem::path path_from_utf8(std::string_view text){return std::filesystem::path(std::u8string(text.begin(),text.end()));}
bool has_scheme(std::string_view address){
    const auto colon=address.find(':');if(colon==address.npos||colon==0)return false;
#if defined(_WIN32)
    if(colon==1&&address.size()>2&&(address[2]=='/'||address[2]=='\\'))return false;
#endif
    const auto alpha=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');};
    if(!alpha(address[0]))return false;
    return std::ranges::all_of(address.substr(1,colon-1),[&](char c){return alpha(c)||(c>='0'&&c<='9')||c=='+'||c=='-'||c=='.';});
}
bool is_gzip(std::string_view bytes){return bytes.size()>=2&&static_cast<unsigned char>(bytes[0])==0x1f&&static_cast<unsigned char>(bytes[1])==0x8b;}
struct Url final {
    CURLU* handle=curl_url();
    Url(){if(!handle)throw std::bad_alloc();}
    ~Url(){curl_url_cleanup(handle);}
    bool set(std::string_view value){
        if(value.find('\0')!=value.npos||!ii::core::valid_utf8(value))return false;
        const std::string text(value);return curl_url_set(handle,CURLUPART_URL,text.c_str(),CURLU_NON_SUPPORT_SCHEME)==CURLUE_OK;
    }
    std::string part(CURLUPart part)const{
        char* value=nullptr;if(curl_url_get(handle,part,&value,0)!=CURLUE_OK)return {};
        std::string result(value);curl_free(value);return result;
    }
    bool remote()const{
        const auto scheme=lower(part(CURLUPART_SCHEME));
        return (scheme=="http"||scheme=="https")&&!part(CURLUPART_HOST).empty()&&
               part(CURLUPART_USER).empty()&&part(CURLUPART_PASSWORD).empty();
    }
};
Result<std::string> unpack(std::string bytes,std::size_t maximum){
    if(bytes.size()>maximum)return ii::core::fail(ErrorCode::full,"Image source exceeds its byte limit");
    if(!is_gzip(bytes))return bytes;
    z_stream stream{};
    if(inflateInit2(&stream,15+16)!=Z_OK)return ii::core::fail(ErrorCode::failed,"Cannot initialize gzip decoder");
    struct End final{z_stream& stream;~End(){inflateEnd(&stream);}}end{stream};
    stream.next_in=reinterpret_cast<Bytef*>(bytes.data());stream.avail_in=static_cast<uInt>(bytes.size());
    std::string output;std::array<char,8192> buffer{};
    for(;;){
        stream.next_out=reinterpret_cast<Bytef*>(buffer.data());stream.avail_out=static_cast<uInt>(buffer.size());
        const int code=inflate(&stream,Z_NO_FLUSH);const auto count=buffer.size()-stream.avail_out;
        if(count>maximum-output.size())return ii::core::fail(ErrorCode::full,"Compressed image exceeds its expanded byte limit");
        output.append(buffer.data(),count);
        if(code==Z_STREAM_END){if(stream.avail_in)return ii::core::fail(ErrorCode::invalid_argument,"Trailing compressed image data");return output;}
        if(code!=Z_OK||(!count&&!stream.avail_in))return ii::core::fail(ErrorCode::invalid_argument,"Invalid compressed image");
    }
}
class RemoteRequest final:public ImageRequest,public std::enable_shared_from_this<RemoteRequest> {
    ii::network::Http http_;
    ii::runtime::EventLoop::Sender sender_;
    ImageCompletion completion_;
    std::string current_;
    std::size_t maximum_;
    unsigned redirects_=0;
    bool finished_=false;
    ii::runtime::TaskToken queued_;
    std::shared_ptr<detail::ImageWorkQueue> work_;
    std::shared_ptr<ImageRequest> unpack_request_;
    ii::runtime::Clock::time_point deadline_=ii::runtime::Clock::now()+std::chrono::seconds(10);
    void finish(Result<std::string> result){
        if(finished_)return;
        finished_=true;auto unpack=std::move(unpack_request_);unpack.reset();auto completion=std::move(completion_);
        if(completion)completion(std::move(result));
    }
    void received(ii::network::HttpResponse response){
        if(finished_)return;
        if(response.timed_out){finish(ii::core::fail(ErrorCode::failed,"Image request timed out"));return;}
        if(response.too_large){finish(ii::core::fail(ErrorCode::full,"Image response exceeds its byte limit"));return;}
        if(!response.error.empty()){finish(ii::core::fail(ErrorCode::failed,response.error));return;}
        if(response.status>=300&&response.status<400){
            const auto location=response.header_fields.get("location");
            if(!location||location->empty()||redirects_++>=8){finish(ii::core::fail(ErrorCode::failed,"Invalid image redirect"));return;}
            Url target;
            if(!target.set(current_)){finish(ii::core::fail(ErrorCode::invalid_argument,"Invalid image URL"));return;}
            const auto previous=lower(target.part(CURLUPART_SCHEME));
            if(!target.set(*location)||!target.remote()||
               (previous=="https"&&lower(target.part(CURLUPART_SCHEME))!="https")){
                finish(ii::core::fail(ErrorCode::unsupported,"Unsafe image redirect"));return;
            }
            current_=target.part(CURLUPART_URL);
            if(!schedule())finish(ii::core::fail(ErrorCode::full,"Image redirect queue is full"));
            return;
        }
        if(!response.ok()){finish(ii::core::fail(ErrorCode::failed,"Image HTTP status "+std::to_string(response.status)));return;}
        if(response.body.empty()){finish(ii::core::fail(ErrorCode::failed,"Received empty image payload"));return;}
        std::string encoding;
        if(work_)if(auto value=response.header_fields.get("content-encoding")){
            encoding=lower(*value);const auto first=encoding.find_first_not_of(" \t"),last=encoding.find_last_not_of(" \t");
            encoding=first==encoding.npos?std::string{}:encoding.substr(first,last-first+1);
            if(encoding!="gzip"&&encoding!="x-gzip"&&encoding!="identity"&& !encoding.empty()){
                finish(ii::core::fail(ErrorCode::unsupported,"Unsupported image HTTP content encoding"));return;
            }
        }
        const bool http_gzip=encoding=="gzip"||encoding=="x-gzip";
        if(http_gzip&&!is_gzip(response.body)){
            finish(ii::core::fail(ErrorCode::invalid_argument,"Invalid gzip HTTP image"));return;
        }
        if(work_&&is_gzip(response.body)){
            auto submitted=work_->submit<std::string>([bytes=std::move(response.body),maximum=maximum_,http_gzip]()mutable{
                auto result=unpack(std::move(bytes),maximum);
                if(result&&http_gzip&&is_gzip(*result))return unpack(std::move(*result),maximum);
                return result;
            },[weak=weak_from_this()](auto result){if(auto self=weak.lock())self->finish(std::move(result));});
            if(!submitted){finish(std::unexpected(submitted.error()));return;}
            unpack_request_=std::move(*submitted);return;
        }
        finish(unpack(std::move(response.body),maximum_));
    }
public:
    RemoteRequest(ii::runtime::EventLoop& loop,std::string url,std::size_t maximum,ImageCompletion completion,
                  std::shared_ptr<detail::ImageWorkQueue> work)
        :http_(loop),sender_(loop.sender()),completion_(std::move(completion)),current_(std::move(url)),maximum_(maximum),work_(std::move(work)){}
    ~RemoteRequest()override{cancel();}
    bool schedule(){
        auto accepted=sender_.post([weak=weak_from_this()]{if(auto self=weak.lock())self->begin();});
        if(!accepted)return false;queued_=accepted.token;return true;
    }
    void begin(){
        queued_={};if(finished_)return;
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline_-ii::runtime::Clock::now());
        if(remaining.count()<=0){finish(ii::core::fail(ErrorCode::failed,"Image request timed out"));return;}
        ii::network::HttpRequest request;request.url=current_;request.timeout=remaining;
        request.initial_response_timeout=std::chrono::seconds(5);request.inactivity_timeout=std::chrono::seconds(5);
        request.maximum_bytes=maximum_;request.accept_compression=!work_;request.follow_redirects=false;
        if(work_)request.headers["Accept-Encoding"]="gzip";
        auto result=http_.request(std::move(request),[weak=weak_from_this()](auto response){if(auto self=weak.lock())self->received(std::move(response));});
        if(!result)finish(ii::core::fail(ErrorCode::failed,result.error().message));
    }
    void cancel()noexcept override{
        if(finished_)return;finished_=true;completion_={};
        (void)sender_.cancel(queued_);queued_={};
        if(unpack_request_){auto request=std::move(unpack_request_);request->cancel();}
        try{http_.cancel();}catch(...){/* A retired request cannot deliver again. */}
    }
};
class NativeSource final:public ImageResourceSource {
    ii::runtime::EventLoop* loop_;
    ImageSourceOptions options_;
    std::shared_ptr<detail::ImageWorkQueue> work_;
    const std::thread::id owner_=std::this_thread::get_id();
    void owner()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Image source requires its owner thread");}
    Result<ImageResource> ready(std::string_view key,std::string bytes){
        auto value=unpack(std::move(bytes),options_.maximum_source_bytes);
        if(!value)return std::unexpected(value.error());
        return ImageResource{std::string(key),std::move(*value)};
    }
    Result<std::string> read_file(const std::filesystem::path& path){
        if(options_.file_reader)return options_.file_reader->read(path,options_.maximum_source_bytes);
        return iiFileProvider::native::File::read(path,options_.maximum_source_bytes);
    }
    Result<ImageResource> resource(std::string_view address){
        constexpr std::string_view icons="qrc:/qt/qml/LVRS/resources/iconset/";
        const auto normalized=address.starts_with(":/")?"qrc"+std::string(address):std::string(address);
        if(normalized.starts_with(icons)){
            std::string_view name(normalized);name.remove_prefix(icons.size());
            if(const auto end=name.find_first_of("?#");end!=name.npos)name=name.substr(0,end);
            if(name.ends_with(".svg"))name.remove_suffix(4);
            auto icon=authored_icon_catalog()->resolve(name);
            if(icon&&*icon){const auto fallback=lvui::detail::authored_icon_render_fallback(*icon);return ready(address,std::string(fallback.value_or((*icon)->svg)));}
        }
        if(options_.resource){auto bytes=options_.resource(address);if(!bytes)return std::unexpected(bytes.error());return ready(address,std::move(*bytes));}
        return ii::core::fail(ErrorCode::not_found,"Unknown image resource");
    }
public:
    NativeSource(ii::runtime::EventLoop* loop,ImageSourceOptions options,std::shared_ptr<detail::ImageWorkQueue> work={})
        :loop_(loop),options_(std::move(options)),work_(std::move(work)){
        if(!options_.maximum_source_bytes||options_.maximum_source_bytes>64*1024*1024)throw std::invalid_argument("Image source byte limit must be 1..67108864");
    }
    Result<ImageResource> read(std::string_view address)override{
        owner();
        try{
            if(address.empty()||address.find('\0')!=address.npos||!ii::core::valid_utf8(address))return ii::core::fail(ErrorCode::invalid_argument,"Invalid image URL");
            const auto folded=lower(address.substr(0,std::min<std::size_t>(address.size(),5)));
            if(address.size()>(folded=="data:"?options_.maximum_source_bytes*2+256:8192))return ii::core::fail(ErrorCode::full,"Image URL exceeds its byte limit");
            if(address.starts_with(":/")||folded.starts_with("qrc:"))return resource(address);
            if(work_&&(folded=="data:"||folded=="file:"||!has_scheme(address)))
                return ImageResource{std::string(address),std::nullopt};
            if(folded=="data:"){
                const auto comma=address.find(',');
                if(comma==address.npos)return ii::core::fail(ErrorCode::invalid_argument,"Malformed image data URL");
                const auto header=lower(address.substr(0,comma));auto payload=address.substr(comma+1);
                auto bytes=header.ends_with(";base64")?ii::network::wire::unbase64(payload):ii::network::wire::percent_decode(payload);
                if(!bytes)return ii::core::fail(ErrorCode::invalid_argument,"Invalid image data encoding");
                return ready(address,std::move(*bytes));
            }
            if(!has_scheme(address)){
                auto bytes=read_file(path_from_utf8(address));if(!bytes)return std::unexpected(bytes.error());
                return ready(address,std::move(*bytes));
            }
            if(folded!="file:"&&!folded.starts_with("http:")&&folded!="https"){
                if(options_.resource)return resource(address);
                return ii::core::fail(ErrorCode::unsupported,"Unsupported image URL scheme");
            }
            Url url;if(!url.set(address))return ii::core::fail(ErrorCode::invalid_argument,"Invalid image URL");
            const auto scheme=lower(url.part(CURLUPART_SCHEME));
            if(scheme=="file"){
                const auto host=lower(url.part(CURLUPART_HOST));auto decoded=ii::network::wire::percent_decode(url.part(CURLUPART_PATH));
                if(!decoded||decoded->find('\0')!=decoded->npos||!ii::core::valid_utf8(*decoded))return ii::core::fail(ErrorCode::invalid_argument,"Invalid image file path");
                auto path=std::move(*decoded);
#if defined(_WIN32)
                if(path.size()>2&&path[0]=='/'&&path[2]==':')path.erase(0,1);
                if(!host.empty()&&host!="localhost")path="//"+host+path;
#else
                if(!host.empty()&&host!="localhost")return ii::core::fail(ErrorCode::invalid_argument,"Nonlocal image file URL");
#endif
                auto bytes=read_file(path_from_utf8(path));if(!bytes)return std::unexpected(bytes.error());
                return ready(address,std::move(*bytes));
            }
            if(!url.remote())return ii::core::fail(ErrorCode::unsupported,"Unsupported or credentialed image URL");
            return ImageResource{url.part(CURLUPART_URL),std::nullopt};
        }catch(const std::exception& error){return ii::core::fail(ErrorCode::failed,error.what(),std::current_exception());}
    }
    Result<std::shared_ptr<ImageRequest>> fetch(std::string_view key,ImageCompletion completion)override{
        owner();
        if(!completion)return ii::core::fail(ErrorCode::invalid_argument,"Image request requires a completion");
        const auto folded=lower(key.substr(0,std::min<std::size_t>(key.size(),5)));
        if(work_&&(folded=="data:"||folded=="file:"||!has_scheme(key))){
            const auto maximum=options_.maximum_source_bytes;auto reader=options_.file_reader;
            return work_->submit<std::string>([address=std::string(key),maximum,reader=std::move(reader)]()->Result<std::string>{
                NativeSource source(nullptr,{.maximum_source_bytes=maximum,.file_reader=reader});
                auto value=source.read(address);
                if(!value)return std::unexpected(value.error());
                if(!value->bytes)return ii::core::fail(ErrorCode::failed,"Image file did not produce bytes");
                return std::move(*value->bytes);
            },std::move(completion));
        }
        if(!loop_)return ii::core::fail(ErrorCode::unsupported,"Image network access requires an application EventLoop");
        Url url;if(!url.set(key)||!url.remote())return ii::core::fail(ErrorCode::invalid_argument,"Invalid remote image URL");
        auto request=std::make_shared<RemoteRequest>(*loop_,url.part(CURLUPART_URL),options_.maximum_source_bytes,std::move(completion),work_);
        if(!request->schedule())return ii::core::fail(ErrorCode::full,"Image request queue is full");
        return std::shared_ptr<ImageRequest>(std::move(request));
    }
};
}
std::shared_ptr<ImageResourceSource> native_image_source(ii::runtime::EventLoop* loop,ImageSourceOptions options){return std::make_shared<NativeSource>(loop,std::move(options));}
std::shared_ptr<ImageResourceSource> detail::native_async_image_source(ii::runtime::EventLoop& loop,std::shared_ptr<ImageWorkQueue> work,ImageSourceOptions options){
    return std::make_shared<NativeSource>(&loop,std::move(options),std::move(work));
}
} // namespace lvui::native
