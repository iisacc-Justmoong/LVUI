#pragma once
#include <LVUI/AuthoredIcons.h>
#include <LVUI/IconCatalog.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace lvui::native {
// One owner-thread cache per control. SVG data is resolved and decoded before
// publishing a candidate, so lookup/raster failures leave its previous image intact.
template<class Decoder>
class NativeIconCache final {
    using Image=typename Decoder::Image;
    const std::thread::id owner_=std::this_thread::get_id();
    std::shared_ptr<const IconSource> source_;
    std::string name_;
    std::uint32_t pixels_=0;
    std::uint64_t revision_=0;
    std::shared_ptr<const Image> image_;
public:
    [[nodiscard]] ii::core::Result<std::shared_ptr<const Image>> resolve(
            std::string_view name,std::shared_ptr<const IconSource> source,double logical_size){
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Icon cache requires its owner thread");
        if(!std::isfinite(logical_size)||logical_size<1||logical_size>256||name.empty()||name.size()>256||!valid_accessibility_text(name))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI icon request");
        try{
            if(!source){
                auto builtin=builtin_icon_catalog();
                if(!builtin)return ii::core::fail(ii::core::ErrorCode::failed,builtin.error().message);
                auto local=(*builtin)->resolve(name);
                if(local)source=*builtin;
                else if(local.error().code==IconErrorCode::not_found)source=design_icon_catalog();
                else return ii::core::fail(ii::core::ErrorCode::invalid_argument,local.error().message);
            }
            const auto pixels=std::uint32_t(std::ceil(logical_size*2));
            for(unsigned attempt=0;attempt<4;++attempt){
                const auto revision=source->revision();
                if(source_==source&&name_==name&&pixels_==pixels&&revision_==revision&&image_)return image_;
                auto asset=source->resolve(name);
                if(source->revision()!=revision)continue;
                if(!asset){const auto code=asset.error().code==IconErrorCode::not_found?ii::core::ErrorCode::not_found:
                    asset.error().code==IconErrorCode::limit_exceeded?ii::core::ErrorCode::out_of_range:ii::core::ErrorCode::invalid_argument;
                    return ii::core::fail(code,asset.error().message);}
                if(!*asset||(*asset)->svg.empty()||(*asset)->svg.size()>1024*1024||
                   (*asset)->svg.find('\0')!=std::string::npos||!ii::core::valid_utf8((*asset)->svg))
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI icon source bytes");
                const auto fallback=::lvui::detail::authored_icon_render_fallback(*asset);
                auto decoded=Decoder::decode(std::shared_ptr<const void>(*asset),fallback.value_or((*asset)->svg),pixels);
                if(source->revision()!=revision)continue;
                if(!decoded)return std::unexpected(decoded.error());
                if(!*decoded)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Icon decoder returned no image");
                auto next_name=std::string(name);
                source_=std::move(source);name_=std::move(next_name);pixels_=pixels;revision_=revision;image_=*decoded;return image_;
            }
            return ii::core::fail(ii::core::ErrorCode::busy,"Icon source changed repeatedly during resolution");
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI icon resolution failed",std::current_exception());}
    }
};
} // namespace lvui::native
