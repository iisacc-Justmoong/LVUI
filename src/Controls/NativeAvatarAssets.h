#pragma once
#include <LVUI/AuthoredIcons.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <memory>
#include <string>
#include <string_view>

namespace lvui::native {
// The design asset is independent of the application-selected raster decoder.
// Replace the captured paths' token alpha with full coverage so a theme can
// supply the color and opacity once when the image is drawn.
[[nodiscard]] inline ii::core::Result<std::shared_ptr<const IconCatalog>>
avatar_placeholder_mask_catalog(){
    static const auto source=[]()->ii::core::Result<std::shared_ptr<const IconCatalog>> {
        auto captured=design_icon_svg("avatar-placeholder");
        if(!captured)return ii::core::fail(ii::core::ErrorCode::not_found,"Missing LVUI Avatar placeholder SVG");
        std::string mask(*captured);
        constexpr std::string_view original="fill=\"white\" fill-opacity=\"0.8\"";
        constexpr std::string_view replacement="fill=\"white\" fill-opacity=\"1\"";
        std::size_t position=0,count=0;
        while((position=mask.find(original,position))!=std::string::npos){
            mask.replace(position,original.size(),replacement);
            position+=replacement.size();++count;
        }
        if(count!=2)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Unexpected LVUI Avatar placeholder SVG");
        auto catalog=IconCatalog::create({{"avatar-placeholder-mask",std::move(mask)}});
        if(!catalog)return ii::core::fail(ii::core::ErrorCode::invalid_argument,catalog.error().message);
        return *catalog;
    }();
    return source;
}
} // namespace lvui::native
