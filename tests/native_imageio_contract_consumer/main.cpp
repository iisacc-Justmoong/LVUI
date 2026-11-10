#include <LVUI/NativeImageResolver.h>
#include <memory>
#include <string_view>
struct Raster { int value; };
struct Resolver final : lvui::native::ImageResolver<Raster> {
    std::shared_ptr<const Raster> image(std::string_view, unsigned, unsigned) const override {
        return std::make_shared<Raster>(42);
    }
};
int main() {
    std::shared_ptr<const lvui::native::ImageResolver<Raster>> resolver = std::make_shared<Resolver>();
    auto result = lvui::native::resolve_image<Raster>("image", resolver, 16, 16);
    return result && result->value == 42 ? 0 : 1;
}
