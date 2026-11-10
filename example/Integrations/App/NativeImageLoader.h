#pragma once

#include <LVUI/NativeImageResolver.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace ii::gui { class RasterImage; }
namespace ii::runtime { class EventLoop; class Runtime; }

namespace lvui::native {

// A source identifies either available encoded bytes or a deferred resource.
// Asynchronous completion must be deferred to the loader's owner thread.
struct ImageResource final {
    std::string key;
    std::optional<std::string> bytes;
};
using ImageCompletion=std::function<void(ii::core::Result<std::string>)>;
class ImageRequest {
public:
    virtual ~ImageRequest()=default;
    virtual void cancel()noexcept=0;
};
class ImageResourceSource {
public:
    virtual ~ImageResourceSource()=default;
    [[nodiscard]] virtual ii::core::Result<ImageResource> read(std::string_view)=0;
    [[nodiscard]] virtual ii::core::Result<std::shared_ptr<ImageRequest>> fetch(std::string_view,ImageCompletion)=0;
};
// Implementations used with a Runtime must permit calls from its workers.
class ImageFileReader {
public:
    virtual ~ImageFileReader()=default;
    [[nodiscard]] virtual ii::core::Result<std::string> read(
        const std::filesystem::path&,std::size_t maximum_bytes)=0;
};
class ImageDecoder {
public:
    virtual ~ImageDecoder()=default;
    [[nodiscard]] virtual ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> decode(
        std::string_view,std::uint32_t svg_width,std::uint32_t svg_height)=0;
};
[[nodiscard]] std::shared_ptr<ImageDecoder> native_image_decoder();
struct ImageSourceOptions final {
    // App-defined namespaces are resolved by the app, not by LVUI.
    std::function<ii::core::Result<std::string>(std::string_view)> resource;
    std::size_t maximum_source_bytes=16*1024*1024;
    std::shared_ptr<ImageFileReader> file_reader;
};
// A supplied loop must outlive the source and its requests. Without one,
// local/data resources remain usable; network requests report an error.
[[nodiscard]] std::shared_ptr<ImageResourceSource> native_image_source(
    ii::runtime::EventLoop* =nullptr,ImageSourceOptions={});

struct ImageCacheLimits final {
    std::size_t source_bytes=64*1024*1024;
    std::size_t raster_bytes=64*1024*1024;
    std::size_t entries=256;
    std::size_t pending_requests=64;
};
enum class ImageLoadStatus { empty,loading,ready,failed };
struct ImageLoad final {
    ImageLoadStatus status=ImageLoadStatus::empty;
    std::shared_ptr<const ii::gui::RasterImage> image;
    std::string error;
};

// Loader operations and delivery belong to one owner thread. Images are
// immutable; status, errors and pending requests are scoped to each address.
class ImageLoader final:public ImageResolver<ii::gui::RasterImage> {
    struct Impl;
public:
    ImageLoader();
    explicit ImageLoader(ii::runtime::EventLoop&,ImageSourceOptions={},ImageCacheLimits={});
    explicit ImageLoader(std::shared_ptr<ImageResourceSource>,ImageCacheLimits={});
    // The loop and Runtime outlive the loader. The loop must be pumped by the
    // owner; file reads and decoding run on Runtime workers without borrowing
    // loader state. An injected decoder must be safe for concurrent calls.
    ImageLoader(ii::runtime::EventLoop&,ii::runtime::Runtime&,ImageSourceOptions={},
                ImageCacheLimits={},std::shared_ptr<ImageDecoder> ={});
    ImageLoader(std::shared_ptr<ImageResourceSource>,ii::runtime::EventLoop&,ii::runtime::Runtime&,
                ImageCacheLimits={},std::shared_ptr<ImageDecoder> ={});
    ~ImageLoader()override;
    ImageLoader(const ImageLoader&)=delete;
    ImageLoader& operator=(const ImageLoader&)=delete;

    [[nodiscard]] ImageLoad load(std::string_view,std::uint32_t svg_width=64,std::uint32_t svg_height=64)const;
    // Convenience for LVUI bindings. Pending and failed resources return null;
    // read load() when the caller also needs state and error details.
    [[nodiscard]] std::shared_ptr<const ii::gui::RasterImage> image(
        std::string_view address,std::uint32_t svg_width=64,std::uint32_t svg_height=64)const override;
    [[nodiscard]] std::uint64_t revision()const;
    // Explicit invalidation permits retries and observes changed local files.
    void invalidate(std::string_view);
    void clear();
private:
    std::shared_ptr<Impl> impl_;
};
} // namespace lvui::native
