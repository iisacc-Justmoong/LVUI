#pragma once
#include "Avatar.h"
#include "NativeImage.h"
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
struct AvatarInput final {
    std::shared_ptr<const ii::gui::RasterImage> image;
    AvatarConfiguration configuration;
    friend bool operator==(const AvatarInput&,const AvatarInput&)=default;
};
inline AvatarSize avatar_size(AvatarSize value)noexcept{return value;}
inline StyleColor avatar_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}

class NativeAvatar final {
    struct State final {
        NativeImage image;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        explicit State(NativeImage value):image(std::move(value)){}
    };
    std::shared_ptr<State> state_;
    explicit NativeAvatar(NativeImage image):state_(std::make_shared<State>(std::move(image))){}
    [[nodiscard]] static ii::core::Result<ImageInput> prepare(const AvatarInput& input){
        Avatar model;if(!model.set_configuration(input.configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Avatar configuration");
        auto configuration=model.image_configuration();
        std::optional<ImageSize> size;if(input.image)size=ImageSize{double(input.image->width()),double(input.image->height())};
        if(!Image::validate(configuration,size))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Avatar image");
        return ImageInput{input.image,std::move(configuration)};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeAvatar> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const AvatarInput& input={}){
        try {
        auto prepared=prepare(input);if(!prepared)return std::unexpected(prepared.error());
        auto image=NativeImage::create(builder,parent,id,*prepared);if(!image)return std::unexpected(image.error());
        if(auto enabled=image->root().set_enabled(input.configuration.enabled);!enabled)return std::unexpected(enabled.error());
        return NativeAvatar(std::move(*image));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Avatar construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->image.root();}
    [[nodiscard]] ii::core::Result<void> set_options(const AvatarInput& input)const {
        try {
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"LVUI Avatar requires its owner thread");
        auto prepared=prepare(input);if(!prepared)return std::unexpected(prepared.error());
        if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"LVUI Avatar revision exhausted");
        const auto revision=++state_->revision;
        if(auto changed=state_->image.set_options(*prepared);!changed)return changed;
        if(state_->revision!=revision)return {};
        return state_->image.root().set_enabled(input.configuration.enabled);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Avatar update failed",std::current_exception());}
    }
};
} // namespace lvui::native
