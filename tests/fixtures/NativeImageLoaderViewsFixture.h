#pragma once
#include "NativeImageLoader.h"
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

struct NativeImageLoaderViewsModel final {
    ii::core::Observable<std::shared_ptr<lvui::native::ImageLoader>> loader{};
    std::shared_ptr<const ii::gui::RasterImage> pixels;
    ii::core::Observable<std::string> url{std::string("https://example.test/first")};
    ii::core::Observable<bool> show{false};
};
