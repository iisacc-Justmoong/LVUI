#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/NativeIconView.h>
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
std::vector<ii::gui::ImageRun> images(const ii::gui::CanvasFrame& frame) {
    auto result = frame.images;
    for (const auto& overlay : frame.overlays)
        result.insert(result.end(), overlay.images.begin(), overlay.images.end());
    return result;
}
struct Source final : lvui::IconSource {
    std::shared_ptr<const lvui::IconAsset> asset;
    mutable std::function<void()> on_resolve;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view) const override {
        if (auto callback = std::exchange(on_resolve, {})) callback();
        return asset;
    }
    std::uint64_t revision() const noexcept override { return 1; }
};
struct RasterSource final : ii::gui::ButtonIconSource {
    std::shared_ptr<const lvui::IconSource> source;
    explicit RasterSource(std::shared_ptr<const lvui::IconSource> value):source(std::move(value)){}
    std::uint64_t revision()const noexcept override{return source->revision();}
    ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name,double size)const override {
        return lvui::native::icon_image_for_logical_size<ii::gui::CachedSvgDecoder>(source,name,size);
    }
};
}

int main() {
    auto default_source=std::make_shared<RasterSource>(lvui::design_icon_catalog());
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {100, 100}));
    std::optional<ii::gui::IconView> icon;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto row = builder.hstack({}, "row", {.alignment = ii::gui::StackAlignment::center});
        if (!row) return std::unexpected(row.error());
        auto result = ii::gui::IconView::create(builder, *row, "symbol", {.name = "brush", .source = default_source});
        if (!result) return std::unexpected(result.error());
        icon = std::move(*result);
        return {};
    }));
    auto widget = icon->root();
    auto frame = take(tree->drawing_snapshot({100, 100}));
    auto runs = images(frame);
    auto snapshot = take(widget.snapshot());
    check(snapshot.accessibility_semantics && snapshot.accessibility_semantics->role == ii::gui::AccessibilityRole::image &&
          snapshot.accessibility_name == "brush", "Icon accessibility semantics differ");
    check(runs.size() == 1 && runs[0].bounds.width == 18 && runs[0].bounds.height == 18 &&
          runs[0].image->width() == 36, "Figma icon default or raster size differs");
    auto expected = take(lvui::native::design_icon_image<ii::gui::CachedSvgDecoder>("brush", 36));
    check(runs[0].image == expected, "Icon did not use the LVUI design asset");

    good(icon->set_options({.name = "previous-frame", .size = 24, .source = default_source, .accessibility_name = "Previous frame"}));
    runs = images(take(tree->drawing_snapshot({100, 100})));
    snapshot = take(widget.snapshot());
    check(runs.size() == 1 && runs[0].bounds.width == 24 && runs[0].bounds.height == 24 &&
          runs[0].image->width() == 48 && snapshot.accessibility_name == "Previous frame",
          "Icon update did not update its geometry, image and accessible name");
    auto previous = runs[0].image;
    check(!icon->set_options({.name = "unknown-icon-for-test", .source = default_source}) &&
          !icon->set_options({.name = "brush", .size = -1, .source = default_source}), "Invalid icon input was accepted");
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image == previous && runs[0].bounds.width == 24 &&
          take(widget.snapshot()).accessibility_name == "Previous frame", "Rejected input changed the current icon");
    auto source = std::make_shared<Source>();
    auto asset = lvui::design_icon_catalog()->resolve("brush");
    check(bool(asset), "Cannot prepare a test icon source");
    source->asset = *asset;
    auto raster_source=std::make_shared<RasterSource>(source);
    check(!icon->set_options({.name = std::string(257, 'x'), .source = raster_source}),
          "An oversized icon name reached a custom source");
    check(!icon->set_options({.name = " \t", .source = raster_source}) &&
          !icon->set_options({.name = "brush", .size = (std::numeric_limits<double>::quiet_NaN)(), .source = default_source}),
          "Invalid icon names or non-finite sizes were accepted");
    source->on_resolve = [&] {
        good(icon->set_options({.name = "brush", .size = 32, .source = default_source, .accessibility_name = "Current"}));
    };
    good(icon->set_options({.name = "outer", .size = 20, .source = raster_source}));
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image->width() == 64 && runs[0].bounds.width == 32 &&
          take(widget.snapshot()).accessibility_name == "Current", "An older reentrant icon update replaced a newer one");
    bool wrong_thread = false;
    std::thread worker([&] { wrong_thread = !icon->set_options({.name = "brush", .source = default_source}); });
    worker.join();
    check(wrong_thread, "Icon accepted updates from another thread");
    good(host.clear_content());
    check(!icon->set_options({.name = "brush", .source = default_source}), "Retired icon accepted an update");
    tree->close();
}
