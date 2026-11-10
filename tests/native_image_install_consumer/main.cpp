#include "NativeImageLoader.h"
#include <iiGUI/Image.h>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {
class Source final : public lvui::native::ImageResourceSource {
public:
    ii::core::Result<lvui::native::ImageResource> read(std::string_view address) override {
        constexpr std::string_view svg =
            R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="4"><rect width="8" height="4" fill="red"/></svg>)";
        return lvui::native::ImageResource{std::string(address), std::string(svg)};
    }

    ii::core::Result<std::shared_ptr<lvui::native::ImageRequest>> fetch(
        std::string_view, lvui::native::ImageCompletion) override {
        return ii::core::fail(ii::core::ErrorCode::unsupported, "No network access");
    }
};
}

int main() {
    lvui::native::ImageLoader loader(std::make_shared<Source>());
    const auto result = loader.load("asset.svg", 12, 6);
    if (result.status != lvui::native::ImageLoadStatus::ready || !result.image ||
        result.image->width() != 12 || result.image->height() != 6) {
        std::cerr << "Relocated ImageIO consumer failed: " << result.error << '\n';
        return 1;
    }
    std::cout << "Relocated LVUI::ImageIO consumer passed\n";
}
