#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lvui::native {
// A view's binding contract is parameterized on the application's image type.
// It has no dependency on a particular graphics, storage or networking SDK.
template<class Image> class ImageResolver {
public:
    virtual ~ImageResolver()=default;
    [[nodiscard]] virtual std::shared_ptr<const Image> image(
        std::string_view,std::uint32_t width=64,std::uint32_t height=64)const=0;
};
// One activation may share a typed slot with descendants and other activations.
template<class Image> struct ImageServices final {
    ii::core::Observable<std::shared_ptr<const ImageResolver<Image>>> resolver{};
};

// These are bounds on a binding request, independent of any renderer. The
// image provider may enforce a stricter budget before allocating resources.
inline constexpr std::uint32_t maximum_image_request_dimension=8192;
inline constexpr std::uint64_t maximum_image_request_pixels=16ull*1024*1024;

template<class Image>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_image(
    std::shared_ptr<const Image> image,const std::shared_ptr<const ImageResolver<Image>>&,double,double){return image;}
template<class Image>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_image(
    std::nullptr_t,const std::shared_ptr<const ImageResolver<Image>>&,double,double){return {};}
template<class Image>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_image(
    std::string_view address,const std::shared_ptr<const ImageResolver<Image>>& resolver,double width,double height){
    if(address.empty())return {};
    if(!resolver)throw std::invalid_argument("An image URL requires an image resolver");
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||
       width>maximum_image_request_dimension||height>maximum_image_request_dimension)
        throw std::invalid_argument("Invalid image raster size");
    const auto w=std::uint32_t(std::ceil(width)),h=std::uint32_t(std::ceil(height));
    if(std::uint64_t(w)*h>maximum_image_request_pixels)throw std::invalid_argument("Image raster exceeds its pixel budget");
    return resolver->image(address,w,h);
}
template<class Image,class Provider>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_scoped_image(
    std::shared_ptr<const Image> image,Provider&&,double,double){return image;}
template<class Image,class Provider>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_scoped_image(
    std::nullptr_t,Provider&&,double,double){return {};}
template<class Image,class Provider>
[[nodiscard]] inline std::shared_ptr<const Image> resolve_scoped_image(
    std::string_view address,Provider&& provider,double width,double height){
    if(address.empty())return {};
    auto resolver=std::invoke(std::forward<Provider>(provider));
    if(!resolver)return {};
    return resolve_image<Image>(address,resolver,width,height);
}
} // namespace lvui::native
