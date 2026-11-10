#pragma once
#include <LVUI/AuthoredIcons.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string_view>
#include <utility>

namespace lvui::native {
// Asset selection and revision checking are independent of any graphics SDK.
// The application supplies Decoder::Image and Decoder::decode(identity, svg, pixels).
template<class Decoder>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const typename Decoder::Image>>
icon_image(std::shared_ptr<const IconSource> source,std::string_view name,std::uint32_t pixels=64){
    if(!pixels||pixels>512)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Icon raster size must be 1..512 pixels");
    if(!source)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Missing icon source");
    try {
        for(unsigned attempt=0;attempt<4;++attempt){
            const auto revision=source->revision();auto asset=source->resolve(name);
            if(source->revision()!=revision)continue;
            if(!asset){
                const auto code=asset.error().code==IconErrorCode::not_found?ii::core::ErrorCode::not_found:
                    asset.error().code==IconErrorCode::limit_exceeded?ii::core::ErrorCode::out_of_range:ii::core::ErrorCode::invalid_argument;
                return ii::core::fail(code,asset.error().message);
            }
            if(!*asset||(*asset)->svg.empty()||(*asset)->svg.size()>1024*1024||
               (*asset)->svg.find('\0')!=std::string::npos||!ii::core::valid_utf8((*asset)->svg))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid icon asset");
            const auto fallback=::lvui::detail::authored_icon_render_fallback(*asset);
            auto image=Decoder::decode(std::shared_ptr<const void>(*asset),fallback.value_or((*asset)->svg),pixels);
            if(source->revision()!=revision)continue;
            if(!image)return std::unexpected(image.error());
            if(!*image)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Icon decoder returned no image");
            return *image;
        }
        return ii::core::fail(ii::core::ErrorCode::busy,"Icon source changed repeatedly during resolution");
    }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Cannot load icon",std::current_exception());}
}
template<class Decoder>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const typename Decoder::Image>>
icon_image_for_logical_size(std::shared_ptr<const IconSource> source,std::string_view name,double size){
    if(name.empty()||name.size()>256||name.find_first_not_of(" \t\r\n")==name.npos||!valid_accessibility_text(name))
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid icon name");
    if(!std::isfinite(size)||size<1||size>256)
        return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Icon logical size must be 1..256");
    return icon_image<Decoder>(std::move(source),name,static_cast<std::uint32_t>(std::ceil(size*2)));
}
template<class Decoder>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const typename Decoder::Image>>
authored_icon_image(std::string_view name,std::uint32_t pixels=64){return icon_image<Decoder>(authored_icon_catalog(),name,pixels);}
template<class Decoder>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const typename Decoder::Image>>
design_icon_image(std::string_view name,std::uint32_t pixels=64){return icon_image<Decoder>(design_icon_catalog(),name,pixels);}
template<class Decoder>
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const typename Decoder::Image>>
design_icon_image_for_logical_size(std::string_view name,double size=design_icon_default_extent){
    return icon_image_for_logical_size<Decoder>(design_icon_catalog(),name,size);
}
} // namespace lvui::native
